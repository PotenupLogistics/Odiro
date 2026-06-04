# Report Serialization Guide

## 1. 목적

Smoke report와 harness report가 Pydantic model, warning/error object, enum, datetime을 안전하게 JSON으로 저장하도록 한다.

## 2. 저장 원칙

* API key 저장 금지
* full generatedPayload 저장 금지
* full episodeSpec 저장 금지
* summary 중심 저장
* Pydantic model은 JSON-safe dict로 변환

## 3. 적용 대상

* OpenAI smoke report
* Ollama smoke report
* UE5 handoff smoke report
* EpisodeSpec controlled smoke report
