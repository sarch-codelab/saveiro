BUILD   := build

CC      := gcc
AS      := as
LD      := ld
OBJCOPY := objcopy

CFLAGS  := -m32 -std=gnu11 -O2 -ffreestanding -fno-pic -fno-pie \
           -fno-stack-protector -fno-builtin -fno-common \
           -fno-asynchronous-unwind-tables -fno-unwind-tables \
           -mno-sse -mno-mmx -mno-80387 -Wall -Wextra -Iinclude
ASFLAGS := --32
LDFLAGS := -m elf_i386 -nostdlib

KERNEL_C    := $(shell find kernel -name '*.c')
KERNEL_S    := $(shell find kernel -name '*.S')
KERNEL_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(KERNEL_C)) \
               $(patsubst %.S,$(BUILD)/%.o,$(KERNEL_S))

.PHONY: all clean image web serve test run shots shots-gui

all: $(BUILD)/saveiro.img

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/boot.bin: boot/boot.S boot/boot.ld
	@mkdir -p $(BUILD)
	$(AS) $(ASFLAGS) boot/boot.S -o $(BUILD)/boot.o
	$(LD) -m elf_i386 -T boot/boot.ld -o $(BUILD)/boot.elf $(BUILD)/boot.o
	$(OBJCOPY) -O binary $(BUILD)/boot.elf $@

$(BUILD)/stage2.bin: boot/stage2.S boot/stage2.ld
	@mkdir -p $(BUILD)
	$(AS) $(ASFLAGS) boot/stage2.S -o $(BUILD)/stage2.o
	$(LD) -m elf_i386 -T boot/stage2.ld -o $(BUILD)/stage2.elf $(BUILD)/stage2.o
	$(OBJCOPY) -O binary $(BUILD)/stage2.elf $@

$(BUILD)/kernel.elf: $(KERNEL_OBJS) boot/kernel.ld
	$(LD) $(LDFLAGS) -T boot/kernel.ld -o $@ $(KERNEL_OBJS)

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/saveiro.img: $(BUILD)/boot.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin tools/mkimage.py
	python3 tools/mkimage.py $(BUILD)/boot.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin $@

image: $(BUILD)/saveiro.img

clean:
	rm -rf $(BUILD) web/assets

v86-deps:
	@test -d web/node_modules/v86 || (cd web && npm install --silent)

web: $(BUILD)/saveiro.img v86-deps
	@mkdir -p web/assets
	cp web/node_modules/v86/build/libv86.js web/assets/
	cp web/node_modules/v86/build/v86.wasm web/assets/
	cp third_party/v86/seabios.bin third_party/v86/vgabios.bin web/assets/
	cp $(BUILD)/saveiro.img web/assets/
	@echo "web/assets listo"

serve: web
	node tools/serve.mjs web 8080

# Arranque nativo, si tienes QEMU instalado
run: $(BUILD)/saveiro.img
	qemu-system-i386 -fda $(BUILD)/saveiro.img -boot a

# Capturas de pantalla para la documentación (requiere Pillow)
shots: $(BUILD)/saveiro.img v86-deps
	node tools/gui-shot.mjs $(BUILD)/shot-comandos comandos
	python3 tools/render-framebuffer.py $(BUILD)/shot-comandos.raw $(BUILD)/shot-comandos-palette.json docs/comandos.png
	node tools/gui-shot.mjs $(BUILD)/shot-test autopruebas
	python3 tools/render-framebuffer.py $(BUILD)/shot-test.raw $(BUILD)/shot-test-palette.json docs/autopruebas.png

# Capturas del escritorio: portada y calculadora
shots-gui: $(BUILD)/saveiro.img v86-deps
	node tools/gui-shot.mjs $(BUILD)/shot-desktop escritorio
	python3 tools/render-framebuffer.py $(BUILD)/shot-desktop.raw $(BUILD)/shot-desktop-palette.json docs/escritorio.png
	node tools/gui-shot.mjs $(BUILD)/shot-calc calculadora
	python3 tools/render-framebuffer.py $(BUILD)/shot-calc.raw $(BUILD)/shot-calc-palette.json docs/calculadora.png

test: $(BUILD)/saveiro.img v86-deps
	node tools/v86-check.mjs
