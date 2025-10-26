
OUT_DIR += /source

OBJS += \
$(OUT_PATH)/source/utils.o \
$(OUT_PATH)/source/app.o \
$(OUT_PATH)/source/app_att.o \
$(OUT_PATH)/source/battery.o \
$(OUT_PATH)/source/cmd_parser.o \
$(OUT_PATH)/source/flash_eep.o \
$(OUT_PATH)/source/scaning.o \
$(OUT_PATH)/source/bthome_scan.o \
$(OUT_PATH)/source/blt_common.o \
$(OUT_PATH)/source/ble.o \
$(OUT_PATH)/source/bthome_beacon.o \
$(OUT_PATH)/source/lcd.o \
$(OUT_PATH)/source/i2c.o \
$(OUT_PATH)/source/lcd_lywsd03mmc.o \
$(OUT_PATH)/source/lcd_lktmzl02.o \
$(OUT_PATH)/source/lcd_zy_zth02pro.o \
$(OUT_PATH)/source/aes_ccm.o \
$(OUT_PATH)/source/main.o

# Each subdirectory must supply rules for building sources it contributes
$(OUT_PATH)/source/%.o: $(PROJECT_PATH)/%.c
	@echo 'Building file: $<'
	@$(TC32_PATH)tc32-elf-gcc $(GCC_FLAGS) $(INCLUDE_PATHS) -c -o"$@" "$<"