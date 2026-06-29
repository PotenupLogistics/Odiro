class BlankPolicy:
    """Minimal policy skeleton for a newly created project."""

    def start(self, request, state=None):
        return {
            "status": "ok",
            "accepted": True,
            "pathStatus": "empty",
            "debug": {
                "reason": "blank_policy_started"
            }
        }

    def decide(self, request, state=None):
        sequence = getattr(request, "sequence", 0)
        if isinstance(request, dict):
            sequence = request.get("sequence", sequence)

        return {
            "sequence": sequence,
            "status": "ok",
            "action": {
                "steering": 0.0,
                "targetSpeedKmh": 0.0,
                "brake": 1.0,
                "direction": "Forward"
            },
            "debug": {
                "selectedPolicy": "BlankPolicy",
                "reason": "hold_position"
            }
        }

    def end(self, request, state=None):
        return {
            "status": "ok",
            "accepted": True,
            "metrics": {},
            "debug": {
                "reason": "blank_policy_ended"
            }
        }


def create_policy():
    return BlankPolicy()
