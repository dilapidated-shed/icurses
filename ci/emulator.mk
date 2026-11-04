# Executed only after the independently provisioned Android 14 emulator boots.
.PHONY: execute
execute:
	test "$$(adb shell getprop ro.kernel.qemu | tr -d '\r')" = 1
	test "$$(adb shell getprop ro.build.version.sdk | tr -d '\r')" = 34
	adb shell getprop ro.build.fingerprint > "$(OUT)/emulator-context.txt"
	adb shell getprop ro.product.cpu.abilist >> "$(OUT)/emulator-context.txt"
	adb shell mkdir -p /data/local/tmp/icurses-exact-source
	adb push "$(OUT)/bundle/." /data/local/tmp/icurses-exact-source/
	adb shell chmod 700 /data/local/tmp/icurses-exact-source/icurses-demo /data/local/tmp/icurses-exact-source/icurses-live /data/local/tmp/icurses-exact-source/ilua /data/local/tmp/icurses-exact-source/literal-probe /data/local/tmp/icurses-exact-source/test-core /data/local/tmp/icurses-exact-source/test-posix /data/local/tmp/icurses-exact-source/test-demo
	adb shell 'cd /data/local/tmp/icurses-exact-source && sha256sum -c SHA256SUMS' > "$(OUT)/emulator-digests.txt"
	adb shell /data/local/tmp/icurses-exact-source/literal-probe
	adb shell /data/local/tmp/icurses-exact-source/test-core > "$(OUT)/emulator-core.txt"
	adb shell /data/local/tmp/icurses-exact-source/test-posix > "$(OUT)/emulator-posix.txt"
	adb shell /data/local/tmp/icurses-exact-source/icurses-demo --identity > "$(OUT)/emulator-runtime.txt"
	adb shell /data/local/tmp/icurses-exact-source/ilua /data/local/tmp/icurses-exact-source/runtime.ilua >> "$(OUT)/emulator-runtime.txt"
	adb shell /data/local/tmp/icurses-exact-source/ilua /data/local/tmp/icurses-exact-source/symbolic-assignment.lua >> "$(OUT)/emulator-runtime.txt"
	adb shell /data/local/tmp/icurses-exact-source/icurses-demo --probe /data/local/tmp/icurses-exact-source/symbolic-assignment.lua >> "$(OUT)/emulator-runtime.txt"
	adb shell /data/local/tmp/icurses-exact-source/ilua /data/local/tmp/icurses-exact-source/policy.ilua /data/local/tmp/icurses-exact-source/table.ilua >> "$(OUT)/emulator-runtime.txt"
	adb shell env IC_PERFORMANCE_PROFILE=android-api34-x86_64-emu64x /data/local/tmp/icurses-exact-source/test-demo /data/local/tmp/icurses-exact-source/icurses-demo /data/local/tmp/icurses-exact-source/table.ilua j > "$(OUT)/emulator-demo.txt"
	adb shell /data/local/tmp/icurses-exact-source/test-demo --live /data/local/tmp/icurses-exact-source/icurses-live /data/local/tmp/icurses-exact-source/icurses-demo /data/local/tmp/icurses-exact-source/table.ilua >> "$(OUT)/emulator-demo.txt"
