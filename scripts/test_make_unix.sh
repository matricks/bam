#!/bin/sh
set -e

repo=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
temp=$(mktemp -d)
trap 'rm -r "$temp"' 0
trap 'exit 1' HUP INT TERM

mkdir -p "$temp/bin"
cat > "$temp/bin/compiler" <<'EOF'
#!/bin/sh
if [ "$1" = "--version" ]; then
	echo "probe:${0##*/}" >> "$TEST_TRACE"
	if [ "$TEST_CASE" = "missing-compiler" ] ||
		{ [ "$TEST_CASE" = "fallback" ] && [ "${0##*/}" = "gcc" ]; }; then
		exit 1
	fi
	exit 0
fi
stage=build
mode_flag=
for arg do
	if [ "$arg" = "src/tools/txt2c.c" ]; then
		stage=helper
	fi
	if [ "$arg" = "-DBAM_STAT_THREAD_MODE=2" ]; then
		mode_flag=yes
	fi
done
if { [ "$stage" = build ] && [ "$mode_flag" != yes ]; } ||
	{ [ "$stage" = helper ] && [ "$mode_flag" = yes ]; }; then
	echo "Incorrect mode flag forwarding in $stage" >&2
	exit 15
fi
echo "$stage" >> "$TEST_TRACE"
if [ "$TEST_CASE" = "$stage" ]; then
	echo "Simulated $stage failure" >&2
	case "$stage" in
		helper) exit 11 ;;
		build) exit 13 ;;
	esac
fi
EOF
cat > "$temp/bin/uname" <<'EOF'
#!/bin/sh
if [ "$TEST_CASE" = "platform" ]; then
	echo "Simulated uname failure" >&2
	exit 14
fi
case "$TEST_CASE" in
	bsd) echo FreeBSD ;;
	*) echo Linux ;;
esac
EOF
chmod +x "$temp/bin/compiler" "$temp/bin/uname"
cp "$temp/bin/compiler" "$temp/bin/gcc"
cp "$temp/bin/compiler" "$temp/bin/clang"

passed=0
for test_case in success helper generate build header-output missing-compiler fallback bsd platform explicit; do
	root="$temp/$test_case"
	mkdir -p "$root/src/tools"
	cp "$repo/make_unix.sh" "$root/make_unix.sh"
	cat > "$root/src/tools/txt2c" <<'EOF'
#!/bin/sh
echo generate >> "$TEST_TRACE"
if [ "$TEST_CASE" = "generate" ]; then
	echo "Simulated generation failure" >&2
	exit 12
fi
echo '/* generated */'
EOF
	chmod +x "$root/src/tools/txt2c"
	if [ "$test_case" = "header-output" ]; then
		mkdir "$root/src/internal_base.h"
	fi

	expected_code=0
	expected_trace='probe:gcc helper generate build'
	case "$test_case" in
		helper) expected_code=11; expected_trace='probe:gcc helper' ;;
		generate) expected_code=12; expected_trace='probe:gcc helper generate' ;;
		build) expected_code=13 ;;
		header-output) expected_code=nonzero; expected_trace='probe:gcc helper' ;;
		missing-compiler) expected_code=1; expected_trace='probe:gcc probe:clang' ;;
		fallback) expected_trace='probe:gcc probe:clang helper generate build' ;;
		bsd) expected_trace='probe:clang helper generate build' ;;
		platform) expected_code=14; expected_trace='' ;;
		explicit) expected_trace='helper generate build' ;;
	esac
	code=0
	(
		cd "$root"
		TEST_CASE=$test_case
		TEST_TRACE="$root/trace"
		CC=
		if [ "$test_case" = "explicit" ]; then
			CC=compiler
		fi
		PATH="$temp/bin:$PATH"
		export TEST_CASE TEST_TRACE CC PATH
		exec sh ./make_unix.sh -DBAM_STAT_THREAD_MODE=2
	) > "$root/output" 2>&1 || code=$?

	actual_trace=''
	if [ -f "$root/trace" ]; then
		actual_trace=$(tr '\n' ' ' < "$root/trace")
		actual_trace=${actual_trace% }
	fi
	if { [ "$expected_code" = nonzero ] && [ "$code" -eq 0 ]; } ||
		{ [ "$expected_code" != nonzero ] && [ "$code" != "$expected_code" ]; } ||
		[ "$actual_trace" != "$expected_trace" ]; then
		echo "FAIL $test_case: exit $code (expected $expected_code), trace '$actual_trace' (expected '$expected_trace')" >&2
		cat "$root/output" >&2
		exit 1
	fi
	echo "PASS $test_case"
	passed=$((passed + 1))
done
echo "$passed Unix build regression tests passed."
