// Script for game
// REFERENCES: https://www.w3schools.com/tags/ref_canvas.asp
// https://medium.com/better-programming/how-to-make-a-simple-game-loop-using-vanilla-javascript-f7f6360f68a2
// https://www.aleksandrhovhannisyan.com/blog/javascript-game-loop/


const DEAD_ZONE  = 0.18;  // rad/s — ignore noise below this
const SCALE      = 470.0; // linear scale factor (tune this)
const AXIS_BIAS  = 3.5;   // diagonal suppression

// Canvas information /////////////////////////////////////////////////////////
const canvas = document.getElementById("gameCanvas");
const ctx = canvas.getContext("2d");

canvas.height = canvas.clientHeight;
canvas.width = canvas.clientWidth;
//canvas.width  = COLS * CELL_PX;
//canvas.height = ROWS * CELL_PX;

// Game logic /////////////////////////////////////////////////////////////////
// Game state

var gameState = "menu";
var p1Score = 0;
var p2Score = 0;
var highScore = Number(localStorage.getItem("highScore") || 0);
var gameOverAudioSent = false;

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
var eagleRadius = 15;

function spawnNewEagle() {
    // choose a random side of screen
    let side = Math.floor(Math.random() * 4);

    let x,y; // variables for eagle coordinates
    let speed; // variable for eagle speed

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

    // Generate speed of eagle (base speed is 2)
    speed = 2 + Math.floor(Math.random() * 3);

    // Save the eagle to the array
    eagles.push({
        x: x,
        y: y,
        speed: speed,
        alive: true
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

function renderEagles() {
    for (let eagle of eagles) {
        // Draw eagle
        ctx.fillStyle = "orange";
        ctx.beginPath();
        ctx.arc(eagle.x, eagle.y, eagleRadius, 0, Math.PI * 2);
        ctx.fill();
    }
}

// Prometheus /////////////////////////////////////////////////////////////////
// TODO maybe implement like health bar or some shit idk
function renderPrometheus() {
    ctx.beginPath();
    // draw a circle halfway down page
    ctx.arc(canvas.width / 2, canvas.height / 2, 50, 0, 2 * Math.PI);
    ctx.fillStyle = "maroon";
    ctx.fill();
}

// Telemetry data 
var pitch = 60; // starting values
var roll = 67;
var latency = 67;
var gesture = "PAUSE";

function updateTelemetryData() {
    document.getElementById("pitchValue").textContent = pitch;
    document.getElementById("rollValue").textContent = roll;
    document.getElementById("latencyValue").textContent = latency;
    document.getElementById("gestureValue").textContent = gesture;
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
            startGame();
        }
    }
};


// Game Functions //////////////////////////////////////////////////////////////////

// Menu
function renderMenu() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    ctx.fillStyle = "white";

    ctx.font = "48px Arial";

    ctx.textAlign = "center";

    ctx.fillText(
        "LIMITED LIVERS",
        canvas.width / 2,
        canvas.height / 2
    );

    ctx.font = "24px Arial";

    ctx.fillText(
        "Show START gesture",
        canvas.width / 2,
        canvas.height / 2 + 60
    );
}

// Start gesture response
function startGame() {

    eagles = [];
    //addPlayer();
    //addPlayer();
    p1Score = 0; 
    p2Score = 0;

    socket.send(JSON.stringify({
        type: "score",
        p1: p1Score,
        p2: p2Score,
        high: highScore
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
}

function drawGame() {
    // Clear screen
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Render Prometheus
    renderPrometheus();
    // Draw player cursor (IF NO WIRELESS)
    renderCursor();
    // Draw players (IF WIRELESS
    renderPlayers();
    // Render eagles
    renderEagles();
}

function renderGameOver() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    ctx.fillStyle = "white";

    ctx.font = "48px Arial";

    ctx.textAlign = "center";

    ctx.fillText(
        "LIMITED LIVERS",
        canvas.width / 2,
        canvas.height / 2
    );

    ctx.font = "24px Arial";

    ctx.fillText(
        "Oops you lost gesture to restart",
        canvas.width / 2,
        canvas.height / 2 + 60
    );
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