// Script for game
// REFERENCES: https://www.w3schools.com/tags/ref_canvas.asp
// https://medium.com/better-programming/how-to-make-a-simple-game-loop-using-vanilla-javascript-f7f6360f68a2
// https://www.aleksandrhovhannisyan.com/blog/javascript-game-loop/

// Canvas information /////////////////////////////////////////////////////////
const canvas = document.getElementById("gameCanvas");
const ctx = canvas.getContext("2d");

canvas.height = canvas.clientHeight;
canvas.width = canvas.clientWidth;

// Game logic /////////////////////////////////////////////////////////////////
// Game state
var gamestate = "menu";

// Player details /////////////////////////////////////////////////////////////
var players = [];
var playerRadius = 10;
var playerCount = 0;

function addPlayer() {
    // New player gets rendered at centre of screen
    // Save the player to the array
    players.push({
        index: playerCount, // Player identifier
        x: canvas.width / 2, // Player x coord
        y: canvas.height / 2, // Y coord
        lastCoordUpdate: performance.now()
    });

    playerCount++;
}

function renderPlayers() {
    for (let player of players) {
        ctx.fillStyle = "blue";
        ctx.beginPath();
        ctx.arc(cursorX, cursorY, 10, 0, 2 * Math.PI);
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
};

socket.onmessage = (event) => {
    let data = JSON.parse(event.data);

    player = players[data.player - 1];
    gy = data.gy;
    gz = data.gz;

    // Calculate time elapsed since player coords last updated
    let currentTime = performance.now();
    let deltaTime = currentTime - player.lastCoordUpdate; // JSON players are 1-indexed

    // Update player position
    player.x += gy * deltaTime;
    player.y += gz * deltaTime;

    player.lastCoordUpdate = currentTime;
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
    addPlayer();
    addPlayer();

    gameState = "running";
}

// Game running
function updateGameplay() {
    // Check if an eagle has died
    for (let eagle of eagles) {
        let cdx = cursorX - eagle.x;
        let cdy = cursorY - eagle.y;
        // Calculate distance of cursor from eagle
        let cursorDist = Math.sqrt(cdx * cdx + cdy * cdy);
        if (cursorDist < eagleRadius) {
            eagle.alive = false;
        }
    }
    // Filter out dead eagles
    eagles = eagles.filter(eagle => eagle.alive);
    // Move alive eagles
    updateEaglePos();
}

function updateGameplayWireless() {
    // Check if an eagle has died
    for (let eagle of eagles) {
        for (let player of players) {
            let cdx = player.x - eagle.x;
            let cdy = player.y - eagle.y;
            // Calcuuate distance of player cursor from eagle
            let playerDist = Math.sqrt(cdx * cdx + cdy * cdy);
            if (playerDist < eagleRadius) {
                eagle.alive = false;
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
        "U LOST LOL press r to restart ig",
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
            updateGameplay();
            drawGame();
            break;

        case "paused":
            break;

        case "gameover":
            renderGameOver();
            break;
    }
}

// Redraws window at 60FPS
window.onload = setInterval(gameLoop, 1000/60);

// Spawns a new eagle every 2 seconds
setInterval(spawnNewEagle, 2000);