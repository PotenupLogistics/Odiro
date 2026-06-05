# 배달 로봇 정책 설계 참고자료 인덱스

> **수집 방침**: 한국 배달 로봇 범용 초기 정책에 신뢰도 높은 자료만 유지
> **수집일**: 2026-05-31
> **연결 정책문서**: `../initial_policy_proposal.md`
> **총 자료 수**: 10개 (PDF 5개 + MD 5개)
> **신뢰 기준**: 한국 법령 / KIRIA 공식 / 학술 알고리즘 논문 (수학적 기반)

---

## 신뢰 자료 3종

### A. 한국 법령 (korea-law/ + korea-reports/)

| 파일 | 내용 | 정책 매핑 |
|---|---|---|
| `korea-law/01_법률19412호_지능형로봇법_일부개정_2023.md` | 지능형로봇법 제40조의2 (속도·무게 상한, KIRIA 인증 의무) + 제40조의4 (책임보험) | §3 우선순위 2, §3 책임보험 |
| `korea-law/법률19412호_지능형로봇법_일부개정_2023.pdf` | 법률 원문 PDF | 동일 |
| `korea-reports/02_도로교통법_실외이동로봇_관련조항.md` | 무게별 최고속도 차등 (≤100kg→15, 100-230kg→10, >230kg→5 km/h) + 보행자 의제 + 책임보험 체계 | §3 우선순위 2, §2-3 횡단보도 |

### B. KIRIA (korea-kiria/)

| 파일 | 내용 | 정책 매핑 |
|---|---|---|
| `korea-kiria/01_KIRIA_운행안전인증_안내.md` | 8개 심사항목: 규격·운행속도 / 겉모양 / 동적특성 / 주변인식 / 비상정지 / 방수 / 횡단보도 / 관제장치 | §3 전반, §2-3, §6 EmergencyShutdown |
| `korea-kiria/02_KIRIA_인증제품_현황_19개모델.md` | 실제 인증 19개 모델: 속도 3~9 km/h, 질량 60~173 kg, 폭 550~775 mm, 등판각 5~15° | §5 파라미터 초기값 |
| `korea-kiria/KIRIA_지능형로봇_보급및확산사업_관리지침.pdf` | 사업 관리 지침 | 정책 직접 매핑 없음 |

### C. 학술 알고리즘 (academic/)

| 파일 | 내용 | 정책 매핑 |
|---|---|---|
| `academic/Fox_Burgard_Thrun_1997_DWA.pdf` | Dynamic Window Approach. Local 회피 알고리즘. 0.1s (10Hz) 평가주기 (파라미터 아님) | §2-2 Local 회피, §2-5 경로계획 |
| `academic/Helbing_Molnar_1995_SocialForce.pdf` | Social Force Model. 보행자를 힘 벡터로 모델링 → DWA 비용함수 통합 | §2-2 DWA 비용함수 |
| `academic/vandenBerg_2008_RVO.pdf` | Reciprocal Velocity Obstacles. 다중 에이전트 협조 회피 | §2-4 로봇 간 협조 |
| `academic/vandenBerg_2011_ORCA.pdf` | Optimal Reciprocal Collision Avoidance. RVO 개선 (진동 방지) | §2-4 로봇 간 협조 |
| `academic/00_학술논문_종합인덱스.md` | 4개 논문 요약 및 정책 매핑 | 전반 |

---

## 3종 자료가 직접 말하는 것 (정책 확정값 근거)

| 확정값 | 출처 | 파일 |
|---|---|---|
| 최대속도 상한: ≤100kg→15, 100-230kg→10, >230kg→5 km/h | 도로교통법 시행규칙 | `02_도로교통법_*.md` |
| 보행자 신호 준수 의무 | 도로교통법 (보행자 의제) | `02_도로교통법_*.md` |
| 신호 없는 횡단보도 → 관제 승인 필수 | KIRIA 인증 운용 제한 | `02_KIRIA_인증제품_*.md` |
| 비상정지 장치 의무 | KIRIA 심사항목 5 | `01_KIRIA_운행안전인증_안내.md` |
| 관제장치(원격제어) 의무 | KIRIA 심사항목 8 | `01_KIRIA_운행안전인증_안내.md` |
| 실제 인증 로봇 속도 3~9 km/h | KIRIA 19개 모델 실측 | `02_KIRIA_인증제품_*.md` |
| 실제 인증 로봇 폭 550~775 mm | KIRIA 19개 모델 실측 | `02_KIRIA_인증제품_*.md` |
| Local 회피 알고리즘: DWA (0.1s 고정) | Fox 1997 | `Fox_Burgard_Thrun_1997_DWA.pdf` |
| 전역 경로: A* | ROS Nav 표준 (DWA 연계) | `Fox_Burgard_Thrun_1997_DWA.pdf` |
| 다중 로봇 협조 회피: RVO/ORCA | van den Berg 2008/2011 | `vandenBerg_*.pdf` |

---

## 3종 자료에 없는 것 (초기가설 처리 필요)

- 감속/서행/정지 거리 수치 (5m, 3m, 1.5m)
- 긴급 정지 거리 수치 (0.5m)
- 보행자 정지 판단 시간 (3초)
- RequestControl 트리거 시간 (10초)
- 객체별 안전거리 배수 (자전거 ×1.5 등)
- 혼잡 밀도 임계값
- confidence_threshold 수치
- Geofencing 구역 (법령 의무 없음)
- roll/pitch 임계 각도 수치
- 파라미터 대부분의 구체 수치
