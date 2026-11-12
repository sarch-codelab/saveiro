#!/usr/bin/env node
/* Captura el escritorio de SaveiroOS desde v86.
 *
 * Arranca la imagen, conduce el escritorio con el ratón (el kernel entiende
 * los eventos del adaptador PS/2 de v86) y vuelca el framebuffer SVGA de
 * 800x600x8 junto con la paleta usada.
 *
 * Uso: node tools/gui-shot.mjs <prefijo> [escenario]
 *   escenario: escritorio | comandos | autopruebas | calculadora
 */

import fs from "node:fs";
import path from "node:path";
import url from "node:url";

const root = path.resolve(url.fileURLToPath(new URL("..", import.meta.url)));
const prefix = process.argv[2] || path.join(root, "build/gui-screen");
const scenario = process.argv[3] || "escritorio";

const { V86 } = await import(
    url.pathToFileURL(path.join(root, "web/node_modules/v86/build/libv86.mjs")).href
);

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

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

let serial = "";
emulator.add_listener("serial0-output-byte", (byte) => {
    serial += String.fromCharCode(byte);
});

async function waitFor(pattern, timeout = 30000) {
    const deadline = Date.now() + timeout;

    while (Date.now() < deadline) {
        if (pattern.test(serial)) {
            return;
        }
        await sleep(40);
    }

    throw new Error(`tiempo agotado esperando ${pattern}`);
}

/* el shell expone `mouseto` para colocar el puntero sin depender de deltas */
async function warp(x, y) {
    serial = "";
    emulator.serial0_send(`\u0001mouseto ${x} ${y}\n`);
    await waitFor(/saveiro:\/ \$|saveiro:\/\$ /, 8000);
}

async function click(x, y) {
    await warp(x, y);
    emulator.bus.send("mouse-click", [true, false, false]);
    await sleep(120);
    emulator.bus.send("mouse-click", [false, false, false]);
    await sleep(240);
}

async function doubleClick(x, y) {
    await warp(x, y);
    for (let n = 0; n < 2; n++) {
        emulator.bus.send("mouse-click", [true, false, false]);
        await sleep(90);
        emulator.bus.send("mouse-click", [false, false, false]);
        await sleep(n === 0 ? 110 : 320);
    }
}

async function type(text) {
    emulator.keyboard_send_text(text, 40);
    await sleep(120 + text.length * 45);
}

/* comandos por el puerto serie: la shell los ejecuta aunque el foco este en
 * otra ventana, y el eco queda visible en el terminal */
async function shell(command) {
    serial = "";
    emulator.serial0_send(command + "\n");
    await waitFor(/saveiro:\/\$ /, 8000);
    await sleep(250);
}

try {
    await waitFor(/saveiro:\/\$ /, 30000);
    await sleep(400);

    if (scenario === "comandos") {
        await shell("clear");
        await type("help\n");
        await sleep(600);
    } else if (scenario === "autopruebas") {
        await shell("clear");
        await type("test\n");
        await sleep(1400);
    } else if (scenario === "calculadora") {
        await click(40, 585);
        await click(60, 504);
        await sleep(300);
        for (const [x, y] of [[552, 234], [714, 402], [606, 234], [660, 402]]) {
            await click(x, y);
        }
        await shell("clear");
        await shell("about");
        await warp(700, 470);
        await sleep(300);
    } else {
        /* escritorio: terminal con informacion, archivos y editor abiertos */
        await click(42, 120);
        await sleep(400);
        await doubleClick(600, 120);
        await sleep(700);
        await shell("clear");
        await shell("about");
        await warp(690, 430);
        await sleep(400);
    }

    const vga = emulator.v86.cpu.devices.vga;
    if (!vga.svga_enabled) {
        throw new Error("el adaptador no está en modo SVGA");
    }

    const width = vga.svga_width;
    const height = vga.svga_height;
    const memory = vga.svga_memory;
    const pixels = Buffer.alloc(width * height);

    for (let i = 0; i < pixels.length; i++) {
        pixels[i] = memory[i] || 0;
    }

    fs.writeFileSync(`${prefix}.raw`, pixels);
    fs.writeFileSync(
        `${prefix}-palette.json`,
        JSON.stringify(Array.from(vga.vga256_palette || []).slice(0, 256))
    );

    console.log(`captura ${width}x${height} en ${prefix}.raw`);
} catch (error) {
    console.error("error: " + error.message);
    console.error(serial.slice(-400));
    process.exitCode = 1;
} finally {
    emulator.destroy();
}
