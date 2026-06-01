# Ollama Failure Diagnostics

## 1. Purpose

This document explains how to diagnose `validation_failed` results from Ollama live smoke runs.

The goal is to inspect model output quality before changing schema, adding OpenAI fallback, or changing the generation API.

## 2. What To Check

- Whether `rawContentPreview` looks like a JSON object.
- Whether JSON extraction succeeded.
- Which `extractedJsonKeys` were present.
- Which required fields were missing.
- Whether enum or type validation errors occurred.
- Whether repair attempts reduced or repeated the same errors.
- Whether provider-level errors occurred before JSON extraction.

## 3. Report Fields

Detailed live smoke reports include:

- `attemptsDetail`
- `rawContentPreview`
- `rawContentLength`
- `jsonExtractionSuccess`
- `extractedJsonPreview`
- `extractedJsonKeys`
- `validationErrors`
- `validationErrorSummary`
- `repairPromptPreview`
- `providerErrorCode`
- `recommendedNextAction`

Full raw output is not stored by default. Use `--include-raw-attempts` only when explicitly needed.

Full extracted JSON is not stored by default. Use `--include-extracted-json` for detailed validation diagnosis.

## 4. Next Action Rules

- If JSON extraction fails, strengthen JSON-only prompt guidance.
- If required fields are missing, strengthen schema summary and required field instructions.
- If enum or type errors occur, provide clearer enum and type constraints in the prompt.
- If the same validation failures remain after repair, inspect repair prompt quality before adding fallback.
- If local models continue to fail after prompt improvements, evaluate OpenAI fallback separately.
- If every attempt has `providerErrorCode=ollama_timeout`, validation did not actually run. Tune timeout or runtime first.

## 5. Constraints

- Do not modify JSON Schema only to satisfy one model output.
- Do not add sample JSON or fixture files during diagnostics.
- Do not create vector DB or embedding index artifacts.
- Do not make pytest or harness depend on a real Ollama server.

## Prompt Hardening Diagnostics

When Ollama returns extractable JSON but `world_config` validation fails, inspect `validationErrorSummary.missingRequiredFields` and `validationErrorSummary.extraFields` first. Missing required nested fields indicate the prompt needs a stronger required checklist. Extra fields indicate the prompt needs stricter allowed-field guidance.

Prompt hardening does not change the JSON Schema and does not create sample JSON. It only changes prompt package content and repair prompt diagnostics.
