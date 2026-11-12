#!/usr/bin/env node
/* Captura el escritorio gráfico de SaveiroOS desde v86.
 *
 * Arranca la imagen, abre el escritorio con `gui`, escribe un comando en el
 * terminal, coloca el ratón sobre el panel de archivos y vuelca el
 * framebuffer SVGA (8 bpp) junto con la paleta usada.
 *
 * Uso: node tools/gui-shot.mjs [prefijo]   (por defecto build/gui-screen)
 */

import fs from "node:fs";
import path from "node:path";
import url from "node:url";

const root = path.resolve(url.fileURLToPath(new URL("..", import.meta.url)));
const prefix = process.argv[2] || path.join(root, "build/gui-screen");

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

async function waitFor(pattern, timeout = 20000) {
    const deadline = Date.now() + timeout;

    while (Date.now() < deadline) {
        if (pattern.test(serial)) {
            return;
        }
        await sleep(50);
    }

    throw new Error(`tiempo agotado esperando ${pattern}`);
}

try {
    await waitFor(/saveiro:\/\$ /, 30000);

    serial = "";
    emulator.serial0_send("gui\n");
    await waitFor(/Escritorio iniciado/, 20000);

    /* un comando dentro de la ventana de terminal */
    serial = "";
    emulator.serial0_send("about\n");
    await waitFor(/saveiro:\/\$ /, 15000);

    /* llevar el ratón hasta el panel de archivos */
    emulator.bus.send("mouse-delta", [120, -30]);
    await sleep(150);
    emulator.bus.send("mouse-delta", [80, -18]);
    await sleep(400);

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
