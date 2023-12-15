#include "movement.h"

// used: sdk entity
#include "../../sdk/entity.h"
// used: cusercmd
#include "../../sdk/datatypes/usercmd.h"

// used: convars
#include "../../core/convars.h"
#include "../../sdk/interfaces/ienginecvar.h"

// used: cheat variables
#include "../../core/variables.h"

// movement correction angles
static QAngle_t angCorrectionView = {};

void F::MISC::MOVEMENT::OnMove(CUserCmd* pCmd, CCSPlayerController* pLocalController, C_CSPlayerPawn* pLocalPawn)
{
	if (!pLocalController->IsPawnAlive())
		return;

	CBaseUserCmdPB* pBaseCmd = pCmd->cmd.pBase;
	if (pBaseCmd == nullptr)
		return;

	// check if player is in noclip or on ladder or in water
	if (const int32_t nMoveType = pLocalPawn->GetMoveType(); nMoveType == MOVETYPE_NOCLIP || nMoveType == MOVETYPE_LADDER || pLocalPawn->GetWaterLevel() >= WL_WAIST)
		return;

	BunnyHop(pCmd, pBaseCmd, pLocalPawn);

	// loop through all tick commands
	//for (int nTick = 0; nTick < pBaseCmd->nTickCount; nTick++)
	//{
	//	CCSGOInputHistoryEntryPB* pInputEntry = pCmd->cmd.inputHistoryField.pRep->tElements[nTick];
	//	if (pInputEntry == nullptr)
	//		continue;

	//	// save view angles for movement correction
	//	angCorrectionView = pInputEntry->pViewCmd->angValue;

	//	// movement correction & anti-untrusted
	//	ValidateUserCommand(pCmd, pBaseCmd, pInputEntry);
	//}
}

void F::MISC::MOVEMENT::BunnyHop(CUserCmd* pCmd, CBaseUserCmdPB* pUserCmd, C_CSPlayerPawn* pLocalPawn)
{
	if (!C_GET(bool, Vars.bAutoBHop) || CONVAR::sv_autobunnyhopping->value.i1)
		return;

	// update random seed
	//MATH::fnRandomSeed(pUserCmd->nRandomSeed);

	//// bypass of possible SMAC/VAC server anticheat detection
	//if (static bool bShouldFakeJump = false; bShouldFakeJump)
	//{
	//	pCmd->buttonStates.nButtonState1 |= IN_JUMP;
	//	bShouldFakeJump = false;
	//}
	//// check is player want to jump
	//else if (pCmd->buttonStates.nButtonState1 & IN_JUMP)
	//{
	//	// check is player on the ground
	//	if (pLocalPawn->GetFlags() & FL_ONGROUND)
	//		// note to fake jump at the next tick
	//		bShouldFakeJump = true;
	//	// check did random jump chance passed
	//	else if (MATH::fnRandomInt(0, 100) <= C_GET(int, Vars.nAutoBHopChance))
	//		pCmd->buttonStates.nButtonState1 &= ~IN_JUMP;
	//}

	// im lazy so yea :D
	if (pLocalPawn->GetFlags() & FL_ONGROUND)
		pCmd->buttonStates.nButtonState1 &= ~IN_JUMP;
}

void F::MISC::MOVEMENT::ValidateUserCommand(CUserCmd* pCmd, CBaseUserCmdPB* pUserCmd, CCSGOInputHistoryEntryPB* pInputEntry)
{
	if (pUserCmd == nullptr)
		return;

	// clamp angle to avoid untrusted angle
	if (C_GET(bool, Vars.bAntiUntrusted))
	{
		if (pInputEntry->pViewCmd->angValue.IsValid())
		{
			pInputEntry->pViewCmd->angValue.Clamp();
			pInputEntry->pViewCmd->angValue.z = 0.f;
		}
		else
		{
			pInputEntry->pViewCmd->angValue = {};
			L_PRINT(LOG_WARNING) << CS_XOR("view angles have a NaN component, the value is reset");
		}
	}

	MovementCorrection(pUserCmd, pInputEntry, angCorrectionView);

	// correct movement buttons while player move have different to buttons values
	// clear all of the move buttons states
	pCmd->buttonStates.nButtonState1 &= (~IN_FORWARD | ~IN_BACK | ~IN_LEFT | ~IN_RIGHT);

	// re-store buttons by active forward/side moves
	if (pUserCmd->flForwardMove > 0.0f)
		pCmd->buttonStates.nButtonState1 |= IN_FORWARD;
	else if (pUserCmd->flForwardMove < 0.0f)
		pCmd->buttonStates.nButtonState1 |= IN_BACK;

	if (pUserCmd->flSideMove > 0.0f)
		pCmd->buttonStates.nButtonState1 |= IN_RIGHT;
	else if (pUserCmd->flSideMove < 0.0f)
		pCmd->buttonStates.nButtonState1 |= IN_LEFT;
		
	if (!pInputEntry->pViewCmd->angValue.IsZero())
	{
		const float flDeltaX = std::remainderf(pInputEntry->pViewCmd->angValue.x - angCorrectionView.x, 360.f);
		const float flDeltaY = std::remainderf(pInputEntry->pViewCmd->angValue.y - angCorrectionView.y, 360.f);

		float flPitch = CONVAR::m_pitch->value.fl;
		float flYaw = CONVAR::m_yaw->value.fl;

		float flSensitivity = CONVAR::sensitivity->value.fl;
		if (flSensitivity == 0.0f)
			flSensitivity = 1.0f;

		pUserCmd->nMousedX = static_cast<short>(flDeltaX / (flSensitivity * flPitch));
		pUserCmd->nMousedY = static_cast<short>(-flDeltaY / (flSensitivity * flYaw));
	}
}

void F::MISC::MOVEMENT::MovementCorrection(CBaseUserCmdPB* pUserCmd, CCSGOInputHistoryEntryPB* pInputEntry, const QAngle_t& angDesiredViewPoint)
{
	if (pUserCmd == nullptr)
		return;

	Vector_t vecForward = {}, vecRight = {}, vecUp = {};
	angDesiredViewPoint.ToDirections(&vecForward, &vecRight, &vecUp);

	// we don't attempt on forward/right roll, and on up pitch/yaw
	vecForward.z = vecRight.z = vecUp.x = vecUp.y = 0.0f;

	vecForward.NormalizeInPlace();
	vecRight.NormalizeInPlace();
	vecUp.NormalizeInPlace();

	Vector_t vecOldForward = {}, vecOldRight = {}, vecOldUp = {};
	pInputEntry->pViewCmd->angValue.ToDirections(&vecOldForward, &vecOldRight, &vecOldUp);

	// we don't attempt on forward/right roll, and on up pitch/yaw
	vecOldForward.z = vecOldRight.z = vecOldUp.x = vecOldUp.y = 0.0f;

	vecOldForward.NormalizeInPlace();
	vecOldRight.NormalizeInPlace();
	vecOldUp.NormalizeInPlace();

	const float flPitchForward = vecForward.x * pUserCmd->flForwardMove;
	const float flYawForward = vecForward.y * pUserCmd->flForwardMove;
	const float flPitchSide = vecRight.x * pUserCmd->flSideMove;
	const float flYawSide = vecRight.y * pUserCmd->flSideMove;
	const float flRollUp = vecUp.z * pUserCmd->flUpMove;

	// solve corrected movement speed
	pUserCmd->flForwardMove = vecOldForward.x * flPitchSide + vecOldForward.y * flYawSide + vecOldForward.x * flPitchForward + vecOldForward.y * flYawForward + vecOldForward.z * flRollUp;
	pUserCmd->flSideMove = vecOldRight.x * flPitchSide + vecOldRight.y * flYawSide + vecOldRight.x * flPitchForward + vecOldRight.y * flYawForward + vecOldRight.z * flRollUp;
	pUserCmd->flUpMove = vecOldUp.x * flYawSide + vecOldUp.y * flPitchSide + vecOldUp.x * flYawForward + vecOldUp.y * flPitchForward + vecOldUp.z * flRollUp;
}
