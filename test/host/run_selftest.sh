#!/bin/sh
# Autoteste do validador de logs (roda no PC, sem placa).
set -e
cd "$(dirname "$0")/../.."
T=$(mktemp -d)
python3 test/host/gen_logs.py "$T/ok"
python3 test/host/gen_logs.py "$T/bad" bad
python3 tools/verify_log.py --veh "$T/ok_veh.log" --ped "$T/ok_ped.log"
if python3 tools/verify_log.py --veh "$T/bad_veh.log" --ped "$T/bad_ped.log"; then
  echo "ERRO: log defeituoso deveria falhar"; exit 1
else
  echo "OK: log defeituoso foi rejeitado como esperado"
fi
