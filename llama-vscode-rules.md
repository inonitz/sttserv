# llama-vscode-rules.md

## System Entrypoint

- Follow identity constraints in `SOUL.md`.
- Follow execution workflow in `AGENTS.md`.

## Tech Stack (Inferred from sttserv / feature-ptt)

- **Target Project**: Speech-to-Text Server (`sttserv`)
- **Active Feature**: Push-to-Talk (`feature-multimodel`)
- **Dependencies**  
  - miniaudio
  - whispercpp
  - util2
  - tracy
  - googletest
  - C++/C/Python

## Anti-Patterns & Constraints

- Never guess APIs.
- Keep code clean, matching codebase style.
- No placeholder comments. Complete tasks fully.
