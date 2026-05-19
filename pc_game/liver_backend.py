import asyncio
import json
import threading
import serial
import http.server
import socketserver
import websockets
from websockets.asyncio.server import serve

# Global for storinng any newly incoming data
new_data = {
    "player": 0,
    "gy": 0,
    "gz":0
}

new_data_lock = threading.Lock()

# HTTP GAME ###################################################################
def start_game_server():
    port = 5000

    serverHandler = http.server.SimpleHTTPRequestHandler

    with socketserver.TCPServer(("", port), serverHandler) as server:
        print("Started webpage")
        server.serve_forever()

# WEBSOCKET SHIT ##############################################################
async def handle_websocket_connection(websocket):
    print("Client connected!")
    
    while True:
        # Copy over new data
        with new_data_lock:
            data = new_data.copy()

        try:
            await websocket.send(json.dumps(data))
            await asyncio.sleep(0.05)
        except websockets.exceptions.ConnectionClosed:
            print("Client disconnected")
            break

# SERIAL PORT SHIT ############################################################
def open_serial_port():
    ser = serial.Serial('COM6', 115200)

    # Read from port repeatedly
    while True:
        line = ser.readline().decode('uft-8').strip()

        if line:
            print(line)
            # load data into dict
            data = json.loads(line)

            # Update the 'new data'
            with new_data_lock:
                if "player" in data:
                    new_data["player"] = data["player"]
                if "gy" in data:
                    new_data["gy"] = data["gy"]
                if "gz" in data:
                    new_data["gz"] = data["gz"]

        else:    
            print("oops smth went wrong reading the serial line")

# MAIN SHIT ###################################################################
async def main():

    # Start game webpage server hosting thread
    threading.Thread(
        target=start_game_server,
        daemon=True # Close up game if backend program ends
    ).start()

    # Start serial port reading thread
    threading.Thread(
        target=open_serial_port,
        daemon=True
    ).start()

    print("Open browser at:")
    print("http://localhost:5000")

    # Start websocket
    async with serve(handle_websocket_connection, "localhost", 6767) as server:
        print("Websocket started on ws://localhost:6767")
        await server.serve_forever()  # Run forever

if __name__ == "__main__":
    asyncio.run(main())


