all: hero5-linux.dtb hero5-recovery.dtb

hero5-recovery.dtb: hero5.dtsi hero5-recovery.dts
	dtc -O dtb hero5-recovery.dts > hero5-recovery.dtb

hero5-linux.dtb: hero5.dtsi hero5-linux.dts
	dtc -O dtb hero5-linux.dts > hero5-linux.dtb
