from fastapi import FastAPI, Request
import uvicorn


app = FastAPI()

# Set to "Stop", "SlowDown", or "Repath" when you want to force a response.
# Set to None to use the normal distance-based test policy.
FORCED_ACTION = "Repath"


@app.post("/policy/decision")
async def decide(request: Request):
    payload = await request.json()

    print("\n=== DeliveryBot Policy Request ===")
    print(payload)

    if FORCED_ACTION is not None:
        response = {
            "schemaVersion": "1.0.0",
            "selectedAction": FORCED_ACTION,
            "reason": f"forced {FORCED_ACTION} test",
        }

        print("=== DeliveryBot Policy Response ===")
        print(response)

        return response

    context = payload.get("policyContext", {})
    has_front_object = context.get("hasFrontObject", False)
    distance_m = context.get("frontObjectDistanceM", 999.0)
    stop_distance_m = context.get("stopDistanceM", 1.2)
    slow_down_distance_m = context.get("slowDownDistanceM", 3.5)
    can_repath = context.get("canRepath", False)
    in_repath_move_grace_time = context.get("inRepathMoveGraceTime", False)

    if not has_front_object:
        action = "None"
        reason = "No front object"
    elif in_repath_move_grace_time:
        action = "SlowDown"
        reason = "Robot is in repath grace time"
    elif distance_m <= stop_distance_m:
        if can_repath:
            action = "Repath"
            reason = "Front object is inside stop distance, request repath"
        else:
            action = "Stop"
            reason = "Front object is inside stop distance"
    elif distance_m <= slow_down_distance_m:
        action = "SlowDown"
        reason = "Front object is inside slowdown distance"
    else:
        action = "None"
        reason = "Front object is far enough"

    response = {
        "schemaVersion": "1.0.0",
        "selectedAction": action,
        "reason": reason,
    }

    print("=== DeliveryBot Policy Response ===")
    print(response)

    return response


if __name__ == "__main__":
    uvicorn.run(app, host="127.0.0.1", port=8000)
