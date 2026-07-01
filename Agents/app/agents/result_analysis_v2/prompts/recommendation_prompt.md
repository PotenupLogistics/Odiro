제공된 analysis context를 바탕으로 결과 분석 recommendation JSON을 생성하십시오.

- system prompt의 근거 제한, 금지사항, public 응답 안전 규칙을 우선합니다.
- recommendations 배열만 포함한 JSON object를 출력합니다.
- 입력에 포함된 patterns, metrics, warnings, refs로 뒷받침되는 항목만 추천합니다.
- evidence는 실제 제공된 episode 참조만 사용합니다.
- target은 "policy" 또는 "environment"만 사용합니다.
- 근거가 부족하거나 원인 분류가 none이면 recommendations는 빈 배열로 둡니다.
