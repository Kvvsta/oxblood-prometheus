// Script for game
// REFERENCES: https://www.w3schools.com/tags/ref_canvas.asp
// https://medium.com/better-programming/how-to-make-a-simple-game-loop-using-vanilla-javascript-f7f6360f68a2
// https://www.aleksandrhovhannisyan.com/blog/javascript-game-loop/


const DEAD_ZONE  = 0.18;  // rad/s — ignore noise below this
const SCALE      = 500.0; // linear scale factor (tune this)
const AXIS_BIAS  = 3.5;   // diagonal suppression

// Canvas information /////////////////////////////////////////////////////////
const canvas = document.getElementById("gameCanvas");
const ctx = canvas.getContext("2d");

canvas.height = canvas.clientHeight;
canvas.width = canvas.clientWidth;
//canvas.width  = COLS * CELL_PX;
//canvas.height = ROWS * CELL_PX;

// Background image
const bgImage = new Image();
bgImage.src = "prometheusoxblood.jpg";

function drawBackground() {
    ctx.drawImage(bgImage, 0, 0, canvas.width, canvas.height);
}

// Game logic /////////////////////////////////////////////////////////////////
// Game state

var gameState = "menu";
var p1Score = 0;
var p2Score = 0;
var highScore = Number(localStorage.getItem("highScore") || 0);
var gameOverAudioSent = false;
var difficultyModifier = 0;

// Player details /////////////////////////////////////////////////////////////

var playerRadius = 10;
var playerCount = 0;

const players = {
    1: {
        x: canvas.width / 2,
        y: canvas.height / 2,
        color: '#e74c3c',
        bias: { gy: 0, gz: 0 }
    },
    2: {
        x: canvas.width / 2,
        y: canvas.height / 2,
        color: '#3498db',
        bias: { gy: 0, gz: 0 }
    }
};

// ── Grid config ──────────────────────────────────────────────
let lastTime = null;


// ── Last known gyro velocity per player (dead reckoning) ────────
const lastGyro = {
    1: { gy: 0, gz: 0 },
    2: { gy: 0, gz: 0 },
};

function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

// ── Apply one gyro sample ─────────────────────────────────────
// Uses actual elapsed time (dt) so the game loop runs at the
// display refresh rate (~60 Hz) independent of the 50 Hz sensor.
// Linear mapping — no cap so fast flicks travel proportionally far.
function mapAxis(v, dt) {
    const mag = Math.abs(v) - DEAD_ZONE;
    if (mag <= 0) return 0;
    return Math.sign(v) * mag * SCALE * dt;
}

function applyGyro(playerID, gy, gz, dt) {
    const p = players[playerID];
    if (!p) return;

    // Adaptive bias correction: the LSM6DSL has a small zero-rate offset
    // (~±0.17 rad/s) that accumulates during continuous motion. When the
    // device is still (both axes within dead zone), slowly update the bias
    // estimate. Subtracting it before the dead zone check means accumulated
    // drift is corrected from the brief pauses between gestures.
    if (Math.abs(gy) < DEAD_ZONE && Math.abs(gz) < DEAD_ZONE) {
    p.bias.gy += (gy - p.bias.gy) * 0.05;
    p.bias.gz += (gz - p.bias.gz) * 0.05;
    }

    gy -= p.bias.gy;
    gz -= p.bias.gz;

    const absY = Math.abs(gy);
    const absZ = Math.abs(gz);

    // Suppress the weaker axis to prevent diagonal drift
    const useZ = absZ > DEAD_ZONE && !(absY > absZ * AXIS_BIAS);
    const useY = absY > DEAD_ZONE && !(absZ > absY * AXIS_BIAS);

    const dx = useZ ? mapAxis(-gz, dt) : 0;
    const dy = useY ? mapAxis(gy,  dt) : 0;

    p.x = clamp(p.x + dx, 0, canvas.width);
    p.y = clamp(p.y + dy, 0, canvas.height);
}

function renderPlayers() {

    for (const player of Object.values(players)) {
        ctx.fillStyle = player.color;
        ctx.beginPath();
        ctx.arc(
            player.x,
            player.y,
            playerRadius,
            0,
            Math.PI * 2
        );
        ctx.fill();
    }
}

// Cursor (laptop mouse for now) //////////////////////////////////////////////
var cursorX = canvas.width / 2;
var cursorY = canvas.height / 2;

// TODO temporarily updating cursor with mouse movement
document.addEventListener("mousemove", (event) => {
    cursorX = event.clientX;
    cursorY = event.clientY;
});

function renderCursor() {
    ctx.fillStyle = "black";
    ctx.beginPath();
    ctx.arc(cursorX, cursorY, 10, 0, 2 * Math.PI);
    ctx.fill();
}

// Gestures (keyboard for now) ////////////////////////////////////////////////
document.addEventListener("keydown", (event) => {

    if (event.key === "s") {
        startGame();
    }

    if (event.key === "p") {

        if (gameState === "running") {
            gameState = "paused";
        }
        else if (gameState === "paused") {
            gameState = "running";
        }
    }

    if (event.key === "r") {
        startGame();
    }
});

// Eagles /////////////////////////////////////////////////////////////////////
var eagles = [];// array of eagles

// eagle animation frames
const frm1 = new Image();
frm1.src = "eagle_sprites/eagle_1.PNG";
const frm2 = new Image();
frm2.src = "eagle_sprites/eagle_2.PNG";
const frm3 = new Image();
frm3.src = "eagle_sprites/eagle_3.PNG";
const frm4 = new Image();
frm4.src = "eagle_sprites/eagle_4.PNG";
const frm5 = new Image();
frm5.src = "eagle_sprites/eagle_5.PNG";
const frm6 = new Image();
frm6.src = "eagle_sprites/eagle_6.PNG";
const frm7 = new Image();
frm7.src = "eagle_sprites/eagle_7.PNG";
const frm8 = new Image();
frm8.src = "eagle_sprites/eagle_8.PNG";
const frm9 = new Image();
frm9.src = "eagle_sprites/eagle_9.PNG";

var eagleFrames = [frm1, frm2, frm3, frm4, frm5, frm6, frm7, frm8, frm9];
var eagleRadius = 15;

const EAGLE_W = 1070; // width of eagle frame PNG in pixels
const EAGLE_H = 962; // height ""
const EAGLE_DRAW_HEIGHT = eagleRadius * 4; // adjust if eagles are too small on screen
const EAGLE_DRAW_WIDTH = EAGLE_DRAW_HEIGHT * (EAGLE_W / EAGLE_H); // preserve aspect ratio

const BASE_EAGLE_FPS = 6; // frame rate at speed=2 (slowest eagle)
const EAGLE_FRAME_COUNT = 9;

function spawnNewEagle() {

    // TODO exit function if game is paused rn
    if (gameState !== "running") {
        return;
    }

    // choose a random side of screen
    let side = Math.floor(Math.random() * 4);

    let x,y; // variables for eagle coordinates
    let speed; // variable for eagle speed
    let facingRight = true; // Eagle image faces right by default

    // Calculate starting coordinates of eagle
    switch (side) {
        case 0:
            // Spawn eagle somewhere along left side of screen
            x = 0;
            y = Math.random() * canvas.height;
            break;
        case 1:
            // Spawn somewhere along right side of canvas
            x = canvas.width;
            y = Math.random() * canvas.height;
            break;
        case 2:
            // Spawn along top of screen
            x = Math.random() * canvas.width;
            y = canvas.height;
            break;
        case 3:
            // Spawn along  bottom of screen
            x = Math.random() * canvas.width;
            y = 0;
            break;
        default:
            x = 0;
            y = 0;
            break;
    }

    // Determine if eagle should be facing left
    if (x > canvas.width / 2) {
        facingRight = false;
    }

    // Generate speed of eagle (base speed is 2)
    speed = 2 + Math.floor(Math.random() * 3) + difficultyModifier;

    // Calculate how long eagle's animation frames should last
    // Note: speed/2 since 2 is the base eagle speed
    let frameLength = 1 / (BASE_EAGLE_FPS * (speed / 2));

    // Save the eagle to the array
    eagles.push({
        x: x,
        y: y,
        speed: speed,
        alive: true,
        frame: 0,
        frameTimer: 0, // Tracks how long the eagle has been animated for
        facingRight: facingRight,
        frameLength: frameLength
    });
}

function updateEaglePos() {
    for (let eagle of eagles) {
        let dx = canvas.width / 2 - eagle.x;
        let dy = canvas.height / 2 - eagle.y;

        // distance to travel to prometheus
        let dist = Math.sqrt(dx * dx + dy * dy);

        if (dist < 50) {
            gameState = "gameover";
        } else {
            eagle.x += (dx / dist) * eagle.speed;
            eagle.y += (dy / dist) * eagle.speed;
        }
    }
}

function advanceEagleFrame(dt) {
    for (let eagle of eagles) {
        eagle.frameTimer += dt;
        // Calculate how many frames the eagle should have flown through
        if (eagle.frameTimer >= eagle.frameLength) {
            eagle.frame = (eagle.frame + 1) % EAGLE_FRAME_COUNT;
            eagle.frameTimer -= eagle.frameLength;
        }
    }
}

function renderEagles() {
    for (let eagle of eagles) {
        const img = eagleFrames[eagle.frame];
        if (!img.complete) continue; // skip if not loaded yet

        ctx.save();
        ctx.translate(eagle.x, eagle.y);

        if (!eagle.facingRight) {
            ctx.scale(-1, 1); // flip horizontally
        }

        ctx.drawImage(
            img,
            -EAGLE_DRAW_WIDTH / 2, // centre the image on the eagle's position
            -EAGLE_DRAW_HEIGHT / 2,
            EAGLE_DRAW_WIDTH,
            EAGLE_DRAW_HEIGHT
        );

        ctx.restore();
    }
    
}

// Prometheus /////////////////////////////////////////////////////////////////
function renderPrometheus() {
    // Small glowing liver — the target eagles head toward
    ctx.save();
    ctx.shadowColor = "rgba(180, 30, 30, 0.9)";
    ctx.shadowBlur = 18;
    ctx.beginPath();
    ctx.arc(canvas.width / 2, canvas.height / 2, 12, 0, 2 * Math.PI);
    ctx.fillStyle = "#8b1a1a";
    ctx.fill();
    ctx.restore();
}

// Connect to the server (MAKE SURE LIVER BACKEND.PY RUNNING)
const socket = new WebSocket('ws://localhost:6767');
socket.onopen = () => {
    document.getElementById("websocketValue").textContent = "YES";

    socket.send(JSON.stringify({
        type: "score",
        p1: p1Score,
        p2: p2Score,
        high: highScore
    }));
};

socket.onmessage = (event) => {
    let data = JSON.parse(event.data);

    // Check if IMU data
    if ("player" in data) {
        lastGyro[data.player] = {
            gy: data.gy,
            gz: data.gz
        };
    } else if ("gesture" in data) {
        // Check if gesture data
        if (data.gesture === "START") {
            startGame();
        }

        if (data.gesture === "PAUSE") {

            if (gameState === "running") {
                gameState = "paused";
            }
            else if (gameState === "paused") {
                gameState = "running";
            }
        }

        if (data.gesture === "RESTART") {
            //startGame();
            // increase speed and add by 2 * 25%
            difficultyModifier += 0.5;
        }
    }
};


// Game Functions //////////////////////////////////////////////////////////////////

function drawTitle() {
    ctx.save();
    ctx.textAlign = "center";
    ctx.font = "72px 'Caesar Dressing'";
    ctx.shadowColor = "rgba(0, 0, 0, 0.9)";
    ctx.shadowBlur = 14;
    ctx.fillStyle = "rgb(240, 218, 160)";
    ctx.fillText("OXBLOOD PROMETHEUS", canvas.width / 2, 90);
    ctx.restore();
}

// Menu
function renderMenu() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    drawBackground();
    drawTitle();

    // Dark banner behind menu text for readability
    const bx = canvas.width / 2;
    const by = canvas.height / 2;
    ctx.save();
    ctx.fillStyle = "rgba(15, 8, 3, 0.7)";
    ctx.roundRect(bx - 480, by - 50, 960, 130, 10);
    ctx.fill();

    ctx.textAlign = "center";
    ctx.shadowColor = "rgba(0, 0, 0, 0.95)";
    ctx.shadowBlur = 8;

    ctx.font = "36px 'Caesar Dressing'";
    ctx.fillStyle = "rgb(240, 218, 160)";
    ctx.fillText("Shoot the eagles before they reach Prometheus.", bx, by + 10);

    ctx.font = "24px 'Caesar Dressing'";
    ctx.fillStyle = "rgb(210, 190, 140)";
    ctx.fillText("Show RED card to begin.  Show ORANGE card to pause.", bx, by + 60);
    ctx.restore();
}

// Start gesture response
function startGame() {

    eagles = [];
    p1Score = 0; 
    p2Score = 0;
    difficultyModifier = 0;

    // Send update score json packet 
    socket.send(JSON.stringify({
        type: "score",
        p1: p1Score,
        p2: p2Score,
        high: highScore
    }));

    // Send audio ready json packet
    socket.send(JSON.stringify({
        type: "audio",
        event: "ready"
    }));


    gameState = "running";
    gameOverAudioSent = false;
}

// Game running
function updateGameplayWireless() {

    let timestamp = performance.now();

    if (lastTime === null) lastTime = timestamp;
    const dt = Math.min((timestamp - lastTime) / 1000, 0.05); // cap at 50ms
    lastTime = timestamp;

    for (const id of Object.keys(players)) {
    const g = lastGyro[id];
    applyGyro(parseInt(id), g.gy, g.gz, dt);
    }

    // Check if an eagle has died
    for (let eagle of eagles) {
        for (const player of Object.values(players)) {
            let cdx = player.x - eagle.x;
            let cdy = player.y - eagle.y;
            // Calcuuate distance of player cursor from eagle
            let playerDist = Math.sqrt(cdx * cdx + cdy * cdy);
            if (playerDist < eagleRadius) {
                eagle.alive = false;

                const playerId = Number(Object.keys(players).find(id => players[id] === player));

                if (playerId === 1) {
                    p1Score++;
                } else if (playerId === 2) {
                    p2Score++;
                }

                const totalScore = p1Score + p2Score;

                if (totalScore > highScore) {
                    highScore = totalScore;
                    localStorage.setItem("highScore", String(highScore));
                }

                socket.send(JSON.stringify({"type":"audio", "event":"eagle_killed"}));

                socket.send(JSON.stringify({
                    type: "score",
                    p1: p1Score,
                    p2: p2Score,
                    high: highScore
                }));
            }
        }
    }

    // Filter out dead eagles
    eagles = eagles.filter(eagle => eagle.alive);
    // Move alive eagles
    updateEaglePos();
    advanceEagleFrame(dt);
}

function drawGame() {
    // Clear screen
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Draw background
    drawBackground();

    // Draw title
    drawTitle();

    // Render Prometheus
    renderPrometheus();
    // Draw player cursor (IF NO WIRELESS)
    renderCursor();
    // Draw players (IF WIRELESS)
    renderPlayers();
    // Render eagles
    renderEagles();
}

function renderPaused() {
    drawGame();

    // Semi-transparent dark overlay
    ctx.fillStyle = "rgba(0, 0, 0, 0.4)";
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    // PAUSED text
    ctx.save();
    ctx.font = "72px 'Caesar Dressing'";
    ctx.textAlign = "center";
    ctx.shadowColor = "rgba(0, 0, 0, 0.95)";
    ctx.shadowBlur = 14;
    ctx.fillStyle = "rgb(240, 218, 160)";
    ctx.fillText("PAUSED", canvas.width / 2, canvas.height / 2);
    ctx.restore();

}

function renderGameOver() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    drawBackground();
    drawTitle();

    const bx = canvas.width / 2;
    const by = canvas.height / 2;
    ctx.save();
    ctx.fillStyle = "rgba(15, 8, 3, 0.7)";
    ctx.roundRect(bx - 340, by - 50, 680, 130, 10);
    ctx.fill();

    ctx.textAlign = "center";
    ctx.shadowColor = "rgba(0, 0, 0, 0.95)";
    ctx.shadowBlur = 8;

    ctx.font = "48px 'Caesar Dressing'";
    ctx.fillStyle = "rgb(240, 218, 160)";
    ctx.fillText("You have failed.", bx, by + 10);

    ctx.font = "24px 'Caesar Dressing'";
    ctx.fillStyle = "rgb(210, 190, 140)";
    ctx.fillText("Show RED card to restart.", bx, by + 60);
    ctx.restore();
}


// GAME LOOP
function gameLoop() {

    switch (gameState) {

        case "menu":
            renderMenu();
            break;

        case "running":
            updateGameplayWireless();
            drawGame();
            break;

        case "paused":
            renderPaused();
            break;

        case "gameover":
            renderGameOver();
            // send audio cue to base node, if not sent already
            if (!gameOverAudioSent) {
                socket.send(JSON.stringify({"type":"audio", "event":"game_over"}));
                gameOverAudioSent = true;
            }
            break;
    }
}

// Redraws window at 60FPS
window.onload = setInterval(gameLoop, 1000/60);

// Spawns a new eagle every 2 seconds
setInterval(spawnNewEagle, 2000);