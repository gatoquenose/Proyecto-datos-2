@.github/copilot-instructions.md

# Claude Code

- Al iniciar sesión lee `docs/HANDOFF.md` y `SPEC.md` una sola vez; no los
  releas salvo que cambien.
- El shell es Windows PowerShell. No uses sintaxis bash ni comandos de Unix.
- Muestra en texto cada comando completo antes de ejecutarlo.
- Antes de editar, presenta el plan y la lista exacta de archivos; espera
  aprobación explícita. No amplíes el alcance sin autorización.
- No firmes commits con `Co-authored-by: Copilot`. Usa el trailer propio de
  Claude Code: `Co-Authored-By: Claude <noreply@anthropic.com>`.
- Ahorra tokens: lee archivos y rangos puntuales, no repitas lecturas ni la
  suite completa si no hubo cambios. Ejecuta la suite completa una vez antes
  del commit.
- No repitas comandos que fallen sin diagnosticar primero la causa.
- No avances de fase ni implementes decisiones aplazadas sin autorización.
