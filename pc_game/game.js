// Script for game
// REFERENCES: https://www.w3schools.com/tags/ref_canvas.asp
// https://medium.com/better-programming/how-to-make-a-simple-game-loop-using-vanilla-javascript-f7f6360f68a2
// https://www.aleksandrhovhannisyan.com/blog/javascript-game-loop/

const canvas = document.getElementById("gameCanvas");
const ctx = canvas.getContext("2d");
canvas.height = window.innerHeight;
canvas.width = window.innerWidth;

// Cursor (laptop mouse for now)
var cursorX = canvas.width / 2;
var cursorY = canvas.height / 2;
document.addEventListener("mousemove", (event) => {
    cursorX = event.clientX;
    cursorY = event.clientY;
});

// Eagles
var eagles = [];// array of eagles

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

    // Calculate speed of eagle (base speed is 2)
    speed = 2 + Math.floor(Math.random() * 3);

    // Save the eagle
    eagles.push({
        x: x,
        y: y,
        speed: speed,
        alive: true
    });

}

function renderEagles() {
    for (let eagle of eagles) {

        let dx = canvas.width / 2 - eagle.x;
        let dy = canvas.height / 2 - eagle.y;

        // distance to travel to prometheus
        let dist = Math.sqrt(dx * dx + dy * dy);

        eagle.x += (dx / dist) * eagle.speed;
        eagle.y += (dy / dist) * eagle.speed;

        // Draw eagle
        ctx.fillStyle = "green";
        ctx.beginPath();
        ctx.arc(eagle.x, eagle.y, 15, 0, Math.PI * 2);
        ctx.fill();
    }
}

// Game loop
function drawGame() {
    // Clear screen
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Insert Prometheus
    ctx.beginPath();
    // draw a circle halfway down page
    ctx.arc(canvas.width / 2, canvas.height / 2, 50, 0, 2 * Math.PI);
    ctx.fillStyle = "red";
    ctx.fill();

    // Draw player cursor
    ctx.fillStyle = "black";
    ctx.beginPath();
    ctx.arc(cursorX, cursorY, 10, 0, 2 * Math.PI);
    ctx.fill();

    // Check if an eagle has died
    for (let eagle of eagles) {
        let cdx = cursorX - eagle.x;
        let cdy = cursorY - eagle.y;
        let cursorDist = Math.sqrt(cdx * cdx + cdy * cdy);
        if (cursorDist < 15) { // eagle is 15 radius
            eagle.alive = false;
        }
    }

    // Filter out dead eagles
    eagles = eagles.filter(eagle => eagle.alive);

    // Render eagles
    renderEagles();
}

// Redraws window at 60FPS
window.onload = setInterval(drawGame, 1000/60);

// Spawns a new eagle every 2 seconds
setInterval(spawnNewEagle, 2000);