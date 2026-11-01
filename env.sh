# Put the project toolchain on PATH for this Git Bash session.
# Usage:  source ./env.sh
MEGA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export MEGA_ROOT
TC="$MEGA_ROOT/toolchain"
for d in "$TC/arm/bin" "$TC/dasm" "$TC/make/bin" "$TC/stella"; do
  [ -d "$d" ] || echo "warning: missing $d - run: py -3 tools/setup_toolchain.py" >&2
done
export PATH="$TC/arm/bin:$TC/dasm:$TC/make/bin:$TC/stella:$PATH"
echo "Toolchain on PATH: arm-none-eabi-gcc, dasm, make, Stella"
