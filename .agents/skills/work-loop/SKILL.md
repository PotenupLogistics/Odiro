---
name: work-loop
description: Plan-Implement-Verify loop for ProtoRobotSim
---

# Work Loop

## Use
- Trigger: feature, change, bug, or work needing plan plus verification
- Skip: simple Q&A, file lookup, terminology

## Loop
- Plan: goal, scope, success condition, related files, `.agents/POLICY.md`, smallest verification
- Artifact: `Docs/plans/PLAN-NNNN-<type>-<title>.md` for durable large work
- Task shape: group similar work into `T01`, `T02` semantic steps with detailed actions, optional manual work, verification
- Implement: small change unit, changed file/function location
- Verify: build, Automation, PIE, or log check
- UE detail: `../ue5-dev/SKILL.md`

## Retry
- Start from first `Error` or `Fatal`
- Resolve root cause before cascade errors

## Report
`.agents/POLICY.md` Style
