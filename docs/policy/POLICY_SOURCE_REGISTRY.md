# POLICY_SOURCE_REGISTRY

## 목적

이 문서는 한국 법·인증·운행 기준 원본 문서를 프로젝트의 정책 근거 자료로 등록하고 관리하기 위한 Source Registry의 기준 문서다.

## 원본 파일 보관 위치

원본 PDF는 `data/sources/raw/korea/`에 보관한다.

## 등록 문서 목록

| Source ID | 제목 | 파일 경로 | 예상 활용 목적 | Status |
| --- | --- | --- | --- | --- |
| KOR-001 | 지능형 로봇 개발 및 보급 촉진법 | `data/sources/raw/korea/KOR-001_지능형로봇법.pdf` | 법적 배경, 실외이동로봇 운행, 인증 근거 확인 | `to_review` |
| KOR-002 | 도로교통법 실외이동로봇 관련 법률 | `data/sources/raw/korea/KOR-002_도로교통법_실외이동로봇.pdf` | 보도 운행, 보행자 지위, 교통 규칙 맥락 확인 | `to_review` |
| KOR-003 | KIRIA 실외이동로봇 운행안전인증 가이드북 | `data/sources/raw/korea/KOR-003_KIRIA_실외이동로봇_운행안전인증_가이드북.pdf` | 속도 정책, 비상정지, 인지 요구사항, 운영자 제어, 안전인증 확인 | `to_review` |
| KOR-004 | 실외이동로봇 운행안전인증 절차 및 기준 등에 관한 고시 | `data/sources/raw/korea/KOR-004_산업통상자원부_운행안전인증_절차_및_기준_고시.pdf` | 인증 절차, 인증 기준, 안전 요구사항 확인 | `to_review` |
| KOR-005 | 도로교통법 제2조 하위법령 운행기준 참고자료 | `data/sources/raw/korea/KOR-005_도로교통법_제2조_하위법령_운행기준_참고자료.pdf` | 용어 정의, 실외이동로봇 정의, 교통 규칙 세부 확인 | `to_review` |

## Status 의미

- `to_review`: 아직 내용 검토 전
- `reviewed`: 내용 검토 후 정책 카드 작성 가능
- `rejected`: 정책 근거로 사용하지 않음

## 다음 단계

1. 원본 문서 내용 검토
2. processed markdown 생성
3. 정책으로 변환 가능한 내용 추출
4. policy knowledge card 작성
5. 하네스 검증 후 RAG 반영
