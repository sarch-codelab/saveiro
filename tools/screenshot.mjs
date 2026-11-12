#!/usr/bin/env node
/* Captura el contenido de la pantalla VGA (80x25 con atributos) en JSON.
 *
 * Uso: node tools/screenshot.mjs <salida.json> [comando ...]
 */

import fs from "node:fs";
import path from "node:path";
import url from "node:url";

const root = path.resolve(url.fileURLToPath(new URL("..", import.meta.url)));
const output = process.argv[2] || path.join(root, "build/screen.json");
const commands = process.argv.slice(3);

const { V86 } = await import(url.pathToFileURL(path.join(root, "web/node_modules/v86/build/libv86.mjs")).href);

function load(file) {
    const buffer = fs.readFileSync(file);
    return buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength);
}

const emulator = new V86({
    wasm_path: path.join(root, "web/node_modules/v86/build/v86.wasm"),
    bios: { buffer: load(path.join(root, "third_party/v86/seabios.bin")) },
    vga_bios: { buffer: load(path.join(root, "third_party/v86/vgabios.bin")) },
    fda: { buffer: load(path.join(root, "build/saveiro.img")) },
    memory_size: 32 * 1024 * 1024,
    autostart: true,
    disable_speaker: true,
});

let serial = "";
let waiter = null;

emulator.add_listener("serial0-output-byte", (byte) => {
    serial += String.fromCharCode(byte);
    if (waiter && serial.endsWith("saveiro:/$ ")) {
        const done = waiter;
        waiter = null;
        done();
    }
});

function waitPrompt(timeout = 25000) {
    return new Promise((resolve, reject) => {
        if (serial.endsWith("saveiro:/$ ")) {
            resolve();
            return;
        }
        const timer = setTimeout(() => {
            waiter = null;
            reject(new Error("tiempo agotado esperando el prompt"));
        }, timeout);
        waiter = () => {
            clearTimeout(timer);
            resolve();
        };
    });
}

try {
    await waitPrompt();

    for (const command of commands) {
        serial = "";
        emulator.serial0_send(command + "\n");
        await waitPrompt();
        await new Promise((resolve) => setTimeout(resolve, 250));
    }

    /* Dejar correr el reloj para que la barra de estado no diga "up 0s". */
    await new Promise((resolve) => setTimeout(resolve, 2200));

    const vga = emulator.v86.cpu.devices.vga.vga_memory;
    const rows = [];

    for (let row = 0; row < 25; row++) {
        const cells = [];
        for (let col = 0; col < 80; col++) {
            const offset = (row * 80 + col) * 2;
            cells.push([vga[offset], vga[offset + 1]]);
        }
        rows.push(cells);
    }

    fs.mkdirSync(path.dirname(output), { recursive: true });
    fs.writeFileSync(output, JSON.stringify(rows));
    console.log(`captura guardada en ${output}`);
    emulator.destroy();
    process.exit(0);
} catch (error) {
    console.error("error: " + error.message);
    console.error(serial);
    process.exit(1);
}
