# SOURCE_PROCESSING_GUIDE

## 목적

raw PDF 문서를 processed Markdown으로 변환하는 목적은 사람이 원본 출처, 추출 상태, 관련 키워드, 정책 설계 후보를 한 파일에서 검토할 수 있게 만드는 것이다. processed Markdown은 자동 정책 카드가 아니라 검토용 중간 산출물이다.

## 원본 PDF를 직접 RAG에 넣지 않는 이유

원본 PDF는 표, 이미지, 조항 구조, 스캔 품질에 따라 텍스트 추출이 불완전할 수 있다. 직접 RAG에 넣으면 추출 오류나 누락이 정책 판단 근거로 사용될 위험이 있다.

## 처리 원칙

processed Markdown도 바로 RAG에 넣지 않는다. 이후 사람이 원본 PDF와 processed Markdown을 대조하고, 검토된 내용만 policy knowledge card로 변환한다.

## 처리 명령

```bash
uv run --with pypdf python scripts/process_sources.py
```

## extractionStatus 의미

- `success`: 텍스트 추출이 안정적으로 완료됨
- `partial`: 텍스트는 추출되었지만 표, 이미지, 조항 구조 등 수동 대조가 필요함
- `failed`: 텍스트 추출이 실패함
- `needs_manual_review`: 추출 결과가 짧거나 불명확해 원문 수동 검토가 필요함

## needs_manual_review 처리 방식

`needs_manual_review` 또는 `partial` 문서는 임의로 보완하지 않는다. 사람이 원본 PDF를 확인한 뒤에만 processed Markdown을 보정하거나 policy knowledge card 후보로 승격한다.

## 수동 검토 체크리스트

processed Markdown이 `partial`인 문서는 `data/sources/review/korea/`의 review checklist를 통해 원본 PDF와 대조한다. 체크리스트에는 원본 위치, 확인한 내용, 연결 가능 정책, 검토 상태를 사람이 직접 기록한다.

검토가 끝나기 전에는 source를 `reviewed`로 변경하지 않는다. `reviewed` 처리된 source만 policy knowledge card 생성 대상으로 삼는다.

## 정책 후보 추출

processed Markdown에서 자동 추출한 정책 후보는 사람이 원본 PDF와 대조하기 위한 보조 자료다. 후보 파일은 `data/sources/review/candidates/korea/`에 보관하며, 모든 후보는 처음에 `needs_pdf_check` 상태로 둔다.

자동 후보는 policy card가 아니다. 원본 PDF 수동 대조 후 `confirmed`로 판단된 후보만 policy knowledge card 생성 대상으로 사용할 수 있다.
