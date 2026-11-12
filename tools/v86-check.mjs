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
}, 150000);

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

    /* --- escritorio gráfico --- */
    const vga = emulator.v86.cpu.devices.vga;
    check(vga.graphical_mode === true && vga.svga_enabled &&
        vga.svga_width === 800 && vga.svga_height === 600,
        "el sistema arranca directamente en modo 800x600x8");

    output = await command("win", /ventanas: 1[\s\S]*Terminal[\s\S]*saveiro:\/\$ /, "win");
    check(/Terminal/.test(output), "el escritorio arranca con la ventana Terminal");

    const warp = async (x, y) => {
        serial = "";
        emulator.serial0_send(`\u0001mouseto ${x} ${y}\n`);
        await waitFor(/saveiro:\/\$ /, 6000, "mouseto");
    };
    const clickAt = async (x, y) => {
        await warp(x, y);
        emulator.bus.send("mouse-click", [true, false, false]);
        await new Promise((resolve) => setTimeout(resolve, 90));
        emulator.bus.send("mouse-click", [false, false, false]);
        await new Promise((resolve) => setTimeout(resolve, 220));
    };
    const doubleClickAt = async (x, y) => {
        await warp(x, y);
        for (let n = 0; n < 2; n++) {
            emulator.bus.send("mouse-click", [true, false, false]);
            await new Promise((resolve) => setTimeout(resolve, 90));
            emulator.bus.send("mouse-click", [false, false, false]);
            await new Promise((resolve) => setTimeout(resolve, n === 0 ? 110 : 260));
        }
    };

    /* menu de inicio -> calculadora */
    await clickAt(40, 585);
    await clickAt(60, 504);
    output = await command("win", /Calculadora[\s\S]*saveiro:\/\$ /, "win calculadora");
    check(/Calculadora/.test(output), "el menu de inicio abre la Calculadora");

    /* arrastrar la ventana por la barra de titulo */
    await warp(600, 158);
    emulator.bus.send("mouse-click", [true, false, false]);
    await new Promise((resolve) => setTimeout(resolve, 180));
    emulator.bus.send("mouse-delta", [-60, -40]);
    await new Promise((resolve) => setTimeout(resolve, 180));
    emulator.bus.send("mouse-delta", [-60, -40]);
    await new Promise((resolve) => setTimeout(resolve, 220));
    emulator.bus.send("mouse-click", [false, false, false]);
    await new Promise((resolve) => setTimeout(resolve, 250));
    output = await command("win", /Calculadora[\s\S]*saveiro:\/\$ /, "win tras arrastrar");
    const dragged = output.match(/Calculadora\s+230x300 en (\d+),(\d+)/);
    check(!!dragged && (Number(dragged[1]) !== 520 || Number(dragged[2]) !== 150),
        `las ventanas se arrastran por la barra de titulo (${dragged ? dragged[1] + "," + dragged[2] : "sin datos"})`);

    /* minimizar y recuperar desde la barra de tareas */
    await clickAt(687, 49);
    output = await command("win", /minimizada[\s\S]*saveiro:\/\$ /, "win minimizada");
    check(/Terminal[\s\S]*minimizada/.test(output), "el boton minimizar oculta la ventana");
    await clickAt(120, 585);
    output = await command("win", /Terminal[\s\S]*visible[\s\S]*saveiro:\/\$ /, "win restaurada");
    check(/Terminal[\s\S]*visible/.test(output), "la barra de tareas restaura la ventana");

    /* archivos -> doble clic -> editor -> escribir -> se guarda en el RAMFS */
    await clickAt(42, 120);
    await doubleClickAt(600, 120);
    output = await command("win", /Editor[\s\S]*saveiro:\/\$ /, "win editor");
    check(/Editor/.test(output), "el doble clic en un archivo abre el editor");

    emulator.keyboard_send_text("prueba gui", 40);
    await new Promise((resolve) => setTimeout(resolve, 500));
    await clickAt(679, 89);            /* cerrar el editor: guarda solo */
    await new Promise((resolve) => setTimeout(resolve, 400));
    serial = "";
    emulator.serial0_send("cat README\n");
    await waitFor(/saveiro:\/\$ /, 8000, "cat README");
    check(/prueba gui/.test(serial), "el editor guarda el texto en el RAMFS");

    output = await command("win", /ventanas: \d+[\s\S]*saveiro:\/\$ /, "win final");
    check(!/Editor/.test(output), "cerrar una ventana la quita del escritorio");

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
