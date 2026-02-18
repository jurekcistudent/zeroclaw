# ZeroClaw Bridge — socket server for full MCU peripheral control
# SPDX-License-Identifier: MPL-2.0

import socket
import threading
from arduino.app_utils import App, Bridge

ZEROCLAW_PORT = 9999


def handle_client(conn):
    try:
        data = conn.recv(1024).decode().strip()
        if not data:
            conn.close()
            return
        parts = data.split()
        if len(parts) < 1:
            conn.sendall(b"error: empty command\n")
            conn.close()
            return
        cmd = parts[0].lower()

        # ── GPIO ──────────────────────────────────────────────
        if cmd == "gpio_write" and len(parts) >= 3:
            pin = int(parts[1])
            value = int(parts[2])
            Bridge.call("digitalWrite", [pin, value])
            conn.sendall(b"ok\n")

        elif cmd == "gpio_read" and len(parts) >= 2:
            pin = int(parts[1])
            val = Bridge.call("digitalRead", [pin])
            conn.sendall(f"{val}\n".encode())

        # ── ADC ───────────────────────────────────────────────
        elif cmd == "adc_read" and len(parts) >= 2:
            channel = int(parts[1])
            val = Bridge.call("analogRead", [channel])
            conn.sendall(f"{val}\n".encode())

        # ── PWM ───────────────────────────────────────────────
        elif cmd == "pwm_write" and len(parts) >= 3:
            pin = int(parts[1])
            duty = int(parts[2])
            result = Bridge.call("analogWrite", [pin, duty])
            if result == -1:
                conn.sendall(b"error: not a PWM pin\n")
            else:
                conn.sendall(b"ok\n")

        # ── I2C ───────────────────────────────────────────────
        elif cmd == "i2c_scan":
            result = Bridge.call("i2cScan", [])
            conn.sendall(f"{result}\n".encode())

        elif cmd == "i2c_transfer" and len(parts) >= 4:
            addr = int(parts[1])
            hex_data = parts[2]
            rx_len = int(parts[3])
            result = Bridge.call("i2cTransfer", [addr, hex_data, rx_len])
            conn.sendall(f"{result}\n".encode())

        # ── SPI ───────────────────────────────────────────────
        elif cmd == "spi_transfer" and len(parts) >= 2:
            hex_data = parts[1]
            result = Bridge.call("spiTransfer", [hex_data])
            conn.sendall(f"{result}\n".encode())

        # ── CAN ───────────────────────────────────────────────
        elif cmd == "can_send" and len(parts) >= 3:
            can_id = int(parts[1])
            hex_data = parts[2]
            result = Bridge.call("canSend", [can_id, hex_data])
            if result == -2:
                conn.sendall(b"error: CAN not yet available\n")
            else:
                conn.sendall(b"ok\n")

        # ── LED Matrix ────────────────────────────────────────
        elif cmd == "led_matrix" and len(parts) >= 2:
            hex_bitmap = parts[1]
            Bridge.call("ledMatrix", [hex_bitmap])
            conn.sendall(b"ok\n")

        # ── RGB LED ───────────────────────────────────────────
        elif cmd == "rgb_led" and len(parts) >= 5:
            led_id = int(parts[1])
            r = int(parts[2])
            g = int(parts[3])
            b = int(parts[4])
            result = Bridge.call("rgbLed", [led_id, r, g, b])
            if result == -1:
                conn.sendall(b"error: invalid LED id (use 3 or 4)\n")
            else:
                conn.sendall(b"ok\n")

        # ── Capabilities ──────────────────────────────────────
        elif cmd == "capabilities":
            result = Bridge.call("capabilities", [])
            conn.sendall(f"{result}\n".encode())

        else:
            conn.sendall(b"error: unknown command\n")

    except Exception as e:
        try:
            conn.sendall(f"error: {e}\n".encode())
        except Exception:
            pass
    finally:
        conn.close()


def accept_loop(server):
    while True:
        try:
            conn, _ = server.accept()
            t = threading.Thread(target=handle_client, args=(conn,))
            t.daemon = True
            t.start()
        except Exception:
            break


def loop():
    App.sleep(1)


def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", ZEROCLAW_PORT))
    server.listen(5)
    server.settimeout(1.0)
    t = threading.Thread(target=accept_loop, args=(server,))
    t.daemon = True
    t.start()
    App.run(user_loop=loop)


if __name__ == "__main__":
    main()
