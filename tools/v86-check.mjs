#!/usr/bin/env node
/* Prueba de humo de SaveiroOS en v86 (sin navegador).
 *
 * Arranca la imagen en un PC emulado, conversa con la shell por el puerto
 * serie y comprueba que los comandos responden como se espera.
 */

import fs from "node:fs";
import path from "node:path";
import url from "node:url";

const root = path.resolve(url.fileURLToPath(new URL("..", import.meta.url)));

const v86Module = path.join(root, "web/node_modules/v86/build/libv86.mjs");
if (!fs.existsSync(v86Module)) {
    console.error("Falta v86. Ejecuta: (cd web && npm install)");
    process.exit(2);
}

const { V86 } = await import(url.pathToFileURL(v86Module).href);

function load(file) {
    const buffer = fs.readFileSync(file);
    return buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength);
}

function requireFile(file, hint) {
    if (!fs.existsSync(file)) {
        console.error(`Falta ${file}. ${hint || ""}`);
        process.exit(2);
    }
}

requireFile(path.join(root, "build/saveiro.img"), "Ejecuta 'make image'.");
requireFile(path.join(root, "third_party/v86/seabios.bin"));
requireFile(path.join(root, "third_party/v86/vgabios.bin"));
requireFile(path.join(root, "web/node_modules/v86/build/v86.wasm"));

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
let resolveWaiter = null;

emulator.add_listener("serial0-output-byte", (byte) => {
    serial += String.fromCharCode(byte);
    if (resolveWaiter && resolveWaiter.pattern.test(serial)) {
        const done = resolveWaiter;
        resolveWaiter = null;
        done.resolve();
    }
});

function waitFor(pattern, timeout = 10000, label = pattern.source) {
    return new Promise((resolve, reject) => {
        if (pattern.test(serial)) {
            resolve();
            return;
        }

        const timer = setTimeout(() => {
            resolveWaiter = null;
            reject(new Error(`tiempo agotado esperando: ${label}`));
        }, timeout);

        resolveWaiter = {
            pattern,
            resolve: () => {
                clearTimeout(timer);
                resolve();
            },
        };

        if (pattern.test(serial)) {
            clearTimeout(timer);
            resolveWaiter = null;
            resolve();
        }
    });
}

let failures = 0;
let passes = 0;

function check(condition, label) {
    if (condition) {
        passes++;
        console.log(`  ✓ ${label}`);
    } else {
        failures++;
        console.log(`  ✗ ${label}`);
    }
}

async function command(cmd, expectation, label) {
    serial = "";
    emulator.serial0_send(cmd + "\n");
    await waitFor(expectation, 12000, label || cmd);
    return serial;
}

function cleanup(code) {
    try {
        emulator.destroy();
    } catch (error) {
        /* ignorado */
    }
    process.exit(code);
}

const watchdog = setTimeout(() => {
    console.error("\nTiempo de espera agotado. Última salida serie:\n");
    console.error(serial);
    cleanup(1);
}, 90000);

try {
    console.log("SaveiroOS · prueba de arranque en v86\n");

    await waitFor(/saveiro:\/\$ $/, 25000, "el prompt de la shell");
    check(/SaveiroOS 0\.1/.test(serial), "el kernel arranca y muestra su banner");
    check(/\[ ok \] memoria detectada/.test(serial), "el mapa E820 del BIOS se lee");

    let output = await command("test", /RESULTADO: \d+\/\d+ pruebas OK[\s\S]*PASS/, "autopruebas");
    check(/RESULTADO: (\d+)\/\1 pruebas OK/.test(output), "las autopruebas pasan sin fallos");

    output = await command("mem", /total detectado : 32 MiB[\s\S]*saveiro:\/\$ /, "mem");
    check(/paginas\s+: 8192 totales/.test(output), "el asignador de páginas conoce 8192 marcos");

    output = await command("ls", /README[\s\S]*saveiro:\/\$ /, "ls");
    check(/motd/.test(output) && /notas/.test(output), "el RAMFS conserva sus archivos");

    output = await command("cat README", /bootloader propio de dos etapas[\s\S]*saveiro:\/\$ /, "cat");
    check(/RAMFS/.test(output), "se puede leer un archivo completo");

    serial = "";
    emulator.serial0_send("spawn 2\n");
    await waitFor(/saveiro:\/\$ $/, 8000, "spawn");
    output = await command("ps", /worker-1[\s\S]*worker-2[\s\S]*saveiro:\/\$ /, "ps");
    check(/worker-1/.test(output) && /worker-2/.test(output), "el planificador ejecuta tareas nuevas");

    output = await command("syscall", /getpid\(\) devolvio[\s\S]*saveiro:\/\$ /, "syscall");
    check(/hola desde int 0x80/.test(output), "int 0x80 despacha la llamada write()");

    output = await command("date", /\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} UTC[\s\S]*saveiro:\/\$ /, "date");
    check(/UTC/.test(output), "el RTC entrega fecha y hora");

    output = await command("write notas hola desde la prueba", /bytes anadidos a notas[\s\S]*saveiro:\/\$ /, "write");
    output = await command("cat notas", /hola desde la prueba[\s\S]*saveiro:\/\$ /, "cat notas");
    check(/hola desde la prueba/.test(output), "RAMFS escribe y lee datos nuevos");

    check(!/PANIC|EXCEPCION/.test(serial), "no hubo pánicos ni excepciones");

    console.log(`\n${failures === 0 ? "PASS" : "FAIL"}: ${passes} comprobaciones OK, ${failures} fallidas`);
} catch (error) {
    console.error("\nError: " + error.message);
    console.error("\n--- salida serie ---\n" + serial);
    failures++;
} finally {
    clearTimeout(watchdog);
    cleanup(failures === 0 ? 0 : 1);
}
