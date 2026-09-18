#pragma onc﻿#pragma once
#include "sdk.hpp"
#include <iostream>
#include "globals.hpp"
#include <math.h>
#include "structs.hpp"
#include <algorithm>
#include "..\Driver\driver.hpp"
#include <unordered_map>
#include <TlHelp32.h>
#include <string>
#include <random>

using namespace Globals;
using namespace Camera;
using namespace UE4;

namespace LAmth
{
	D3DMATRIX matrix(FVector rot, FVector origin = FVector(0, 0, 0)) {
		float radPitch = (rot.x * float(M_PI) / 180.f);
		float radYaw = (rot.y * float(M_PI) / 180.f);
		float radRoll = (rot.z * float(M_PI) / 180.f);

		float SP = sinf(radPitch);
		float CP = cosf(radPitch);
		float SY = sinf(radYaw);
		float CY = cosf(radYaw);
		float SR = sinf(radRoll);
		float CR = cosf(radRoll);

		D3DMATRIX matrix;
		matrix.m[0][0] = CP * CY;
		matrix.m[0][1] = CP * SY;
		matrix.m[0][2] = SP;
		matrix.m[0][3] = 0.f;

		matrix.m[1][0] = SR * SP * CY - CR * SY;
		matrix.m[1][1] = SR * SP * SY + CR * CY;
		matrix.m[1][2] = -SR * CP;
		matrix.m[1][3] = 0.f;

		matrix.m[2][0] = -(CR * SP * CY + SR * SY);
		matrix.m[2][1] = CY * SR - CR * SP * SY;
		matrix.m[2][2] = CR * CP;
		matrix.m[2][3] = 0.f;

		matrix.m[3][0] = origin.x;
		matrix.m[3][1] = origin.y;
		matrix.m[3][2] = origin.z;
		matrix.m[3][3] = 1.f;

		return matrix;
	}

	D3DMATRIX MatrixMultiplication(D3DMATRIX pM1, D3DMATRIX pM2)
	{
		D3DMATRIX pOut;
		pOut._11 = pM1._11 * pM2._11 + pM1._12 * pM2._21 + pM1._13 * pM2._31 + pM1._14 * pM2._41;
		pOut._12 = pM1._11 * pM2._12 + pM1._12 * pM2._22 + pM1._13 * pM2._32 + pM1._14 * pM2._42;
		pOut._13 = pM1._11 * pM2._13 + pM1._12 * pM2._23 + pM1._13 * pM2._33 + pM1._14 * pM2._43;
		pOut._14 = pM1._11 * pM2._14 + pM1._12 * pM2._24 + pM1._13 * pM2._34 + pM1._14 * pM2._44;
		pOut._21 = pM1._21 * pM2._11 + pM1._22 * pM2._21 + pM1._23 * pM2._31 + pM1._24 * pM2._41;
		pOut._22 = pM1._21 * pM2._12 + pM1._22 * pM2._22 + pM1._23 * pM2._32 + pM1._24 * pM2._42;
		pOut._23 = pM1._21 * pM2._13 + pM1._22 * pM2._23 + pM1._23 * pM2._33 + pM1._24 * pM2._43;
		pOut._24 = pM1._21 * pM2._14 + pM1._22 * pM2._24 + pM1._23 * pM2._34 + pM1._24 * pM2._44;
		pOut._31 = pM1._31 * pM2._11 + pM1._32 * pM2._21 + pM1._33 * pM2._31 + pM1._34 * pM2._41;
		pOut._32 = pM1._31 * pM2._12 + pM1._32 * pM2._22 + pM1._33 * pM2._32 + pM1._34 * pM2._42;
		pOut._33 = pM1._31 * pM2._13 + pM1._32 * pM2._23 + pM1._33 * pM2._33 + pM1._34 * pM2._43;
		pOut._34 = pM1._31 * pM2._14 + pM1._32 * pM2._24 + pM1._33 * pM2._34 + pM1._34 * pM2._44;
		pOut._41 = pM1._41 * pM2._11 + pM1._42 * pM2._21 + pM1._43 * pM2._31 + pM1._44 * pM2._41;
		pOut._42 = pM1._41 * pM2._12 + pM1._42 * pM2._22 + pM1._43 * pM2._32 + pM1._44 * pM2._42;
		pOut._43 = pM1._41 * pM2._13 + pM1._42 * pM2._23 + pM1._43 * pM2._33 + pM1._44 * pM2._43;
		pOut._44 = pM1._41 * pM2._14 + pM1._42 * pM2._24 + pM1._43 * pM2._34 + pM1._44 * pM2._44;

		return pOut;
	}
}



GWorld* UWorld;
GGameInstance* UGameInstance;
GLocalPlayer* ULocalPlayer;
GPlayerController* APlayerController;
GPawn* APawn;
GPrivatePawn* APrivatePawn;
GULevel* ULevel;
GUSkeletalMeshComponent* USkeletalMeshComponent;

int iAimKey{ 0 };

bool cached = false;
uintptr_t WorldPtr;
struct FLinearColor {
	float R; // 0x00(0x04)
	float G; // 0x04(0x04)
	float B; // 0x08(0x04)
	float A; // 0x0c(0x04)
};
struct FMatrix {
	struct FVector XPlane;
	struct FVector YPlane;
	struct FVector ZPlane;
	struct FVector WPlane;
};
#define uint unsigned int
#define ushort unsigned short
#define ulong unsigned long
std::string get_fname(int key)
{
	uint chunkOffset = (uint)((int)(key) >> 16);
	ushort nameOffset = (ushort)key;
	std::uint64_t namePoolChunk = readv<std::uint64_t>((std::uintptr_t)(virtualbase + offsets::fnamepool + ((chunkOffset + 2) * 8)));
	
	
	std::uint64_t entryOffset = namePoolChunk + (ulong)(4 * nameOffset);
	FNameEntry nameEntry = readv<FNameEntry>(entryOffset);

	auto name = nameEntry.AnsiName;
	std::uintptr_t nameKey = readv<uintptr_t>(virtualaddy + 0x0);
	if (nameEntry.Header.Len <= 365)
	for (std::uint16_t i = 0; i < nameEntry.Header.Len; i++)
	{
		BYTE b = i & 3;
		name[i] ^= nameEntry.Header.Len ^ *((LPBYTE)&nameKey + b);
	}

	return name;
}

FVector fhgfsdhkfshdghfsd205(FVector src, FVector dst)
{
	FVector angle;
	angle.x = -atan2f(dst.x - src.x, dst.y - src.y) / M_PI * 180.0f + 180.0f;
	angle.y = asinf((dst.z - src.z) / src.Distance(dst)) * 180.0f / M_PI;
	angle.z = 0.0f;

	return angle;
}

FVector CaadadalcAngle(FVector src, FVector dst)
{
	FVector angle;
	FVector delta = FVector((src.x - dst.x), (src.y - dst.y), (src.z - dst.z));

	double hyp = sqrtf(delta.x * delta.x + delta.y * delta.y);

	angle.x = atanf(delta.z / hyp) * (180.0f / hyp);
	angle.y = atanf(delta.y / delta.x) * (180.0f / M_PI);
	angle.z = 0;
	if (delta.x >= 0.0) angle.y += 180.0f;

	return angle;
}

void Clamp(FVector& Ang) {
	if (Ang.x < 0.f)
		Ang.x += 360.f;

	if (Ang.x > 360.f)
		Ang.x -= 360.f;

	if (Ang.y < 0.f) Ang.y += 360.f;
	if (Ang.y > 360.f) Ang.y -= 360.f;
	Ang.z = 0.f;
}

void LAPNRMLZE(FVector& in)
{
	if (in.x > 89.f) in.x -= 360.f;
	else if (in.x < -89.f) in.x += 360.f;

	while (in.y > 180)in.y -= 360;
	while (in.y < -180)in.y += 360;
	in.z = 0;
}

FVector LAPSMTHAM(FVector Camera_rotation, FVector Target, float SmoothFactor)
{
	FVector diff = Target - Camera_rotation;
	LAPNRMLZE(diff);
	return Camera_rotation + diff / SmoothFactor;
}

void NormalizeAngles(FVector& angle)
{
	while (angle.x > 89.0f)
		angle.x -= 180.f;

	while (angle.x < -89.0f)
		angle.x += 180.f;

	while (angle.y > 180.f)
		angle.y -= 360.f;

	while (angle.y < -180.f)
		angle.y += 360.f;
}

void RCS(FVector Target, FVector Camera_rotation, float SmoothFactor) {


	FVector ConvertRotation = Camera_rotation;
	LAPNRMLZE(ConvertRotation);

	auto ControlRotation = readv<FVector>(PlayerController + 0x448);
	FVector DeltaRotation = ConvertRotation - ControlRotation;
	LAPNRMLZE(DeltaRotation);


	ConvertRotation = Target - (DeltaRotation * SmoothFactor);
	LAPNRMLZE(ConvertRotation);

	FVector Smoothed = LAPSMTHAM(Camera_rotation, ConvertRotation, SmoothFactor);
	Smoothed -= (DeltaRotation / SmoothFactor);
	Clamp(Smoothed);
	LAPNRMLZE(Smoothed);
	write<FVector>(PlayerController + 0x448, (FVector)Smoothed);
	return;
}


std::string karakterismi(std::string in)
{
	if (in.find(skCrypt("Training")) != std::string::npos)
		return std::string(skCrypt("NPC"));
	if (in.find(skCrypt("BountyHunter_PC_C")) != std::string::npos)
		return std::string(skCrypt("Fade"));
	if (in.find(skCrypt("Stealth_PC_C")) != std::string::npos)
		return std::string(skCrypt("Yoru"));
	if (in.find(skCrypt("Pandemic_PC_C")) != std::string::npos)
		return std::string(skCrypt("Viper"));
	if (in.find(skCrypt("Hunter_PC_C")) != std::string::npos)
		return std::string(skCrypt("Sova"));
	if (in.find(skCrypt("Guide_PC_C")) != std::string::npos)
		return std::string(skCrypt("Skye"));
	if (in.find(skCrypt("Thorne_PC_C")) != std::string::npos)
		return std::string(skCrypt("Sage"));
	if (in.find(skCrypt("Vampire_PC_C")) != std::string::npos)
		return std::string(skCrypt("Reyna"));
	if (in.find(skCrypt("Clay_PC_C")) != std::string::npos)
		return std::string(skCrypt("Raze"));
	if (in.find(skCrypt("Phoenix_PC_C")) != std::string::npos)
		return std::string(skCrypt("Phoenix"));
	if (in.find(skCrypt("Wraith_PC_C")) != std::string::npos)
		return std::string(skCrypt("Omen"));
	if (in.find(skCrypt("Sprinter_PC_C")) != std::string::npos)
		return std::string(skCrypt("Neon"));
	if (in.find(skCrypt("Killjoy_PC_C")) != std::string::npos)
		return std::string(skCrypt("Killjoy"));
	if (in.find(skCrypt("Grenadier_PC_C")) != std::string::npos)
		return std::string(skCrypt("Kayo"));
	if (in.find(skCrypt("Wushu_PC_C")) != std::string::npos)
		return std::string(skCrypt("Jett"));
	if (in.find(skCrypt("Gumshoe_PC_C")) != std::string::npos)
		return std::string(skCrypt("Cypher"));
	if (in.find(skCrypt("Deadeye_PC_C")) != std::string::npos)
		return std::string(skCrypt("Chamber"));
	if (in.find(skCrypt("Sarge_PC_C")) != std::string::npos)
		return std::string(skCrypt("Brimstone"));
	if (in.find(skCrypt("Breach_PC_C")) != std::string::npos)
		return std::string(skCrypt("Breach"));
	if (in.find(skCrypt("Rift_TargetingForm_PC_C")) != std::string::npos)
		return std::string(skCrypt("Astra"));
	if (in.find(skCrypt("Rift_PC_C")) != std::string::npos)
		return std::string(skCrypt("Astra"));
	if (in.find(skCrypt("Mage_PC_C")) != std::string::npos)
		return std::string(skCrypt("Harbor"));
	if (in.find(skCrypt("AggroBot_PC_C")) != std::string::npos)
		return std::string(skCrypt("Gekko"));
	if (in.find(skCrypt("AggroBot_PC_C")) != std::string::npos)
		return std::string(skCrypt("Gekko"));
	if (in.find(skCrypt("Cable_PC_C")) != std::string::npos)
		return std::string(skCrypt("DeadLock"));
	else
		return std::string(skCrypt("N/A"));
}
std::string weaponismi(std::string in)
{
	if (in.find(skCrypt("Ability_Melee_Base_C")) != std::string::npos)
		return std::string(skCrypt("Knife"));
	if (in.find(skCrypt("BasePistol_C")) != std::string::npos)
		return std::string(skCrypt("Classic"));
	if (in.find(skCrypt("TrainingBotBasePistol_C")) != std::string::npos)
		return std::string(skCrypt("Classic"));
	if (in.find(skCrypt("SawedOffShotgun_C")) != std::string::npos)
		return std::string(skCrypt("Shorty"));
	if (in.find(skCrypt("AutomaticPistol_C")) != std::string::npos)
		return std::string(skCrypt("Frenzy"));
	if (in.find(skCrypt("LugerPistol_C")) != std::string::npos)
		return std::string(skCrypt("Ghost"));
	if (in.find(skCrypt("RevolverPistol_C")) != std::string::npos)
		return std::string(skCrypt("Sheriff"));
	if (in.find(skCrypt("Vector_C")) != std::string::npos)
		return std::string(skCrypt("Stinger"));
	if (in.find(skCrypt("SubMachineGun_MP5_C")) != std::string::npos)
		return std::string(skCrypt("Spectre"));
	if (in.find(skCrypt("PumpShotgun_C")) != std::string::npos)
		return std::string(skCrypt("Bucky"));
	if (in.find(skCrypt("AssaultRifle_Burst_C")) != std::string::npos)
		return std::string(skCrypt("Bulldog"));
	if (in.find(skCrypt("DMR_C")) != std::string::npos)
		return std::string(skCrypt("Guardian"));
	if (in.find(skCrypt("AssaultRifle_ACR_C")) != std::string::npos)
		return std::string(skCrypt("Phantom"));
	if (in.find(skCrypt("AssaultRifle_AK_C")) != std::string::npos)
		return std::string(skCrypt("Vandal"));
	if (in.find(skCrypt("LeverSniperRifle_C")) != std::string::npos)
		return std::string(skCrypt("Marshall"));
	if (in.find(skCrypt("BoltSniper_C")) != std::string::npos)
		return std::string(skCrypt("Operator"));
	if (in.find(skCrypt("LightMachineGun_C")) != std::string::npos)
		return std::string(skCrypt("Ares"));
	if (in.find(skCrypt("HeavyMachineGun_C")) != std::string::npos)
		return std::string(skCrypt("Odin"));
	if (in.find(skCrypt("Bomb_C")) != std::string::npos)
		return std::string(skCrypt("Spike"));
	if (in.find(skCrypt("Pawn_Gumshoe_Q_PossessableCamera_C")) != std::string::npos)
		return std::string(skCrypt("Cyper Camera"));
	if (in.find(skCrypt("Pawn_Hunter_E_Drone_Prototype_Balance_C")) != std::string::npos)
		return std::string(skCrypt("Sova Drone"));
	else
		return std::string(skCrypt("N/A"));
}
std::string randomisim(size_t length = 0)
{
	static const std::string allowed_chars{ "0123456789abcdefghjklmnoprstuvqyzABCDEFGHIJKLMNOPRSTUVYZ" };

	static thread_local std::default_random_engine randomEngine(std::random_device{}());
	static thread_local std::uniform_int_distribution<int> randomDistribution(0, allowed_chars.size() - 1);

	std::string id(length ? length : 32, '\0');

	for (std::string::value_type& c : id) {
		c = allowed_chars[randomDistribution(randomEngine)];
	}

	return id;
}
auto CacheGame() -> void
{
	while (true)
	{

		std::vector<ValEntity> CachedList;
		std::vector<ValEntity2> CachedList2;
		WorldPtr = GetWorld(virtualaddy);
		auto ULevelPtr = UWorld->ULevel(WorldPtr);
		auto UGameInstancePtr = UWorld->GameInstance(WorldPtr);
		auto ULocalPlayerPtr = UGameInstance->ULocalPlayer(UGameInstancePtr);
		auto APlayerControllerPtr = ULocalPlayer->APlayerController(ULocalPlayerPtr);
		PlayerController = APlayerControllerPtr;
		PlayerCameraManager = APlayerController->APlayerCameraManager(APlayerControllerPtr);
		auto MyHUD = APlayerController->AHUD(APlayerControllerPtr);

		auto APawnPtr = APlayerController->APawn(APlayerControllerPtr);
		MyTeamID = APawn->TeamID(APawnPtr);
		LocalPlayer = APawnPtr;
		if (APawnPtr != 0)
		{
			MyUniqueID = APawn->UniqueID(APawnPtr);
			MyRelativeLocation = APawn->RelativeLocation(APawnPtr);
		}
		

		if (MyHUD != 0)
		{

			auto PlayerArray = ULevel->AActorArray(ULevelPtr);
			for (uint32_t i = 0; i < PlayerArray.Count; ++i)
			{
				auto Pawns = PlayerArray[i];

				if (Pawns != APawnPtr)
				{
					if (MyUniqueID == APawn->UniqueID(Pawns))
					{
						uintptr_t mesh = readv<uintptr_t>(Pawns + 0x438);
						uintptr_t root_comp = readv<uintptr_t>(Pawns + 0x238);
						uintptr_t bone_array = readv<uintptr_t>(mesh + 0x5D8);
						ValEntity Entities{ Pawns, mesh, bone_array, root_comp };
						CachedList.push_back(Entities);
					}
				}
				if (Pawns != APawnPtr)
				{
					if (Settings::Visuals::bItem || Settings::Visuals::bTimer)
					{
						std::string agent_name = get_fname(readv<int>(Pawns + 0x18));

						if (strstr(agent_name.c_str(), skCrypt("EquippableGroundPickup_C"))||strstr(agent_name.c_str(), skCrypt("TimedBomb_C"))) {
							ValEntity2 Entities{ Pawns ,false };
							CachedList2.push_back(Entities);
						}
					}
				}
			}

			ValList.clear();
			ValList2.clear();
			ValList = CachedList;
			ValList2 = CachedList2;
		}
		Sleep(1000);

	}
}



D3DCOLOR FLOAT4TOD3DCOLOR(float Col[])
{
	ImU32 col32_no_alpha = ImGui::ColorConvertFloat4ToU32(ImVec4(Col[0], Col[1], Col[2], Col[3]));
	float a = (col32_no_alpha >> 24) & 255;
	float r = (col32_no_alpha >> 16) & 255;
	float g = (col32_no_alpha >> 8) & 255;
	float b = col32_no_alpha & 255;
	return D3DCOLOR_ARGB((int)a, (int)r, (int)g, (int)b);
}
void renderBoneLine(FVector first_bone_position, FVector second_bone_position, D3DCOLOR asgvash) {
	FVector first_bone_screen_position = UE4::SDK::ProjectWorldToScreen(first_bone_position);
	ImVec2 fist_screen_position = ImVec2(first_bone_screen_position.x, first_bone_screen_position.y);
	FVector second_bone_screen_position = UE4::SDK::ProjectWorldToScreen(second_bone_position);
	ImVec2 second_screen_position = ImVec2(second_bone_screen_position.x, second_bone_screen_position.y);
	ImGui::GetForegroundDrawList()->AddLine(fist_screen_position, second_screen_position, asgvash, 1);
}

void renderBones(uintptr_t SkeletalMesh, D3DCOLOR asgvash) {
	auto bone_count = USkeletalMeshComponent->BoneCount(SkeletalMesh);
	FVector head_position = UE4::SDK::GetEntityBone(SkeletalMesh, 8);
	FVector neck_position;
	FVector chest_position = UE4::SDK::GetEntityBone(SkeletalMesh, 6);
	FVector l_upper_arm_position;
	FVector l_fore_arm_position;
	FVector l_hand_position;
	FVector r_upper_arm_position;
	FVector r_fore_arm_position;
	FVector r_hand_position;
	FVector stomach_position = UE4::SDK::GetEntityBone(SkeletalMesh, 4);
	FVector pelvis_position = UE4::SDK::GetEntityBone(SkeletalMesh, 3);
	FVector l_thigh_position;
	FVector l_knee_position;
	FVector l_foot_position;
	FVector r_thigh_position;
	FVector r_knee_position;
	FVector r_foot_position;
	if (bone_count == 104) {


		// MALE
		neck_position = UE4::SDK::GetEntityBone(SkeletalMesh, 21);

		l_upper_arm_position = UE4::SDK::GetEntityBone(SkeletalMesh, 23);
		l_fore_arm_position = UE4::SDK::GetEntityBone(SkeletalMesh, 24);
		l_hand_position = UE4::SDK::GetEntityBone(SkeletalMesh, 25);

		r_upper_arm_position = UE4::SDK::GetEntityBone(SkeletalMesh, 49);
		r_fore_arm_position = UE4::SDK::GetEntityBone(SkeletalMesh, 50);
		r_hand_position = UE4::SDK::GetEntityBone(SkeletalMesh, 51);

		l_thigh_position = UE4::SDK::GetEntityBone(SkeletalMesh, 77);
		l_knee_position = UE4::SDK::GetEntityBone(SkeletalMesh, 78);
		l_foot_position = UE4::SDK::GetEntityBone(SkeletalMesh, 80);

		r_thigh_position = UE4::SDK::GetEntityBone(SkeletalMesh, 84);
		r_knee_position = UE4::SDK::GetEntityBone(SkeletalMesh, 85);
		r_foot_position = UE4::SDK::GetEntityBone(SkeletalMesh, 87);
	}
	else if (bone_count == 101) { // FEMALE

		// FEMALE
		neck_position = UE4::SDK::GetEntityBone(SkeletalMesh, 21);

		l_upper_arm_position = UE4::SDK::GetEntityBone(SkeletalMesh, 23);
		l_fore_arm_position = UE4::SDK::GetEntityBone(SkeletalMesh, 24);
		l_hand_position = UE4::SDK::GetEntityBone(SkeletalMesh, 25);

		r_upper_arm_position = UE4::SDK::GetEntityBone(SkeletalMesh, 49);
		r_fore_arm_position = UE4::SDK::GetEntityBone(SkeletalMesh, 50);
		r_hand_position = UE4::SDK::GetEntityBone(SkeletalMesh, 51);

		l_thigh_position = UE4::SDK::GetEntityBone(SkeletalMesh, 75);
		l_knee_position = UE4::SDK::GetEntityBone(SkeletalMesh, 76);
		l_foot_position = UE4::SDK::GetEntityBone(SkeletalMesh, 78);

		r_thigh_position = UE4::SDK::GetEntityBone(SkeletalMesh, 82);
		r_knee_position = UE4::SDK::GetEntityBone(SkeletalMesh, 83);
		r_foot_position = UE4::SDK::GetEntityBone(SkeletalMesh, 85);
	}
	else if (bone_count == 103) { // BOT
		neck_position = UE4::SDK::GetEntityBone(SkeletalMesh, 9);

		l_upper_arm_position = UE4::SDK::GetEntityBone(SkeletalMesh, 33);
		l_fore_arm_position = UE4::SDK::GetEntityBone(SkeletalMesh, 30);
		l_hand_position = UE4::SDK::GetEntityBone(SkeletalMesh, 32);

		r_upper_arm_position = UE4::SDK::GetEntityBone(SkeletalMesh, 58);
		r_fore_arm_position = UE4::SDK::GetEntityBone(SkeletalMesh, 55);
		r_hand_position = UE4::SDK::GetEntityBone(SkeletalMesh, 57);

		l_thigh_position = UE4::SDK::GetEntityBone(SkeletalMesh, 63);
		l_knee_position = UE4::SDK::GetEntityBone(SkeletalMesh, 65);
		l_foot_position = UE4::SDK::GetEntityBone(SkeletalMesh, 69);

		r_thigh_position = UE4::SDK::GetEntityBone(SkeletalMesh, 77);
		r_knee_position = UE4::SDK::GetEntityBone(SkeletalMesh, 79);
		r_foot_position = UE4::SDK::GetEntityBone(SkeletalMesh, 83);
	}
	else {
		return;
	}
	renderBoneLine(head_position, neck_position, asgvash);
	renderBoneLine(neck_position, chest_position, asgvash);
	renderBoneLine(neck_position, l_upper_arm_position, asgvash);
	renderBoneLine(l_upper_arm_position, l_fore_arm_position, asgvash);
	renderBoneLine(l_fore_arm_position, l_hand_position, asgvash);
	renderBoneLine(neck_position, r_upper_arm_position, asgvash);
	renderBoneLine(r_upper_arm_position, r_fore_arm_position, asgvash);
	renderBoneLine(r_fore_arm_position, r_hand_position, asgvash);
	renderBoneLine(chest_position, stomach_position, asgvash);
	renderBoneLine(stomach_position, pelvis_position, asgvash);
	renderBoneLine(pelvis_position, l_thigh_position, asgvash);
	renderBoneLine(l_thigh_position, l_knee_position, asgvash);
	renderBoneLine(l_knee_position, l_foot_position, asgvash);
	renderBoneLine(pelvis_position, r_thigh_position, asgvash);
	renderBoneLine(r_thigh_position, r_knee_position, asgvash);
	renderBoneLine(r_knee_position, r_foot_position, asgvash);
}

FVector GetMidPoint(FVector V1, FVector V2)
{
	FVector Mid;
	Mid.x = (V1.x + V2.x) / 2;
	Mid.y = (V1.y + V2.y) / 2;
	Mid.z = (V1.z + V2.z) / 2;
	return Mid;
}
void BoxCac(FVector Root, D3DCOLOR ESPColor,int height,int weight, int Thickness)
{
	int xz = weight;
	int uzunluk = height;
	FVector Pos0, Pos1, Pos2, Pos3, Pos4, Pos5, Pos6, Pos7, Pos8;
	Pos0 = Root;
	Pos0.z = Pos0.z;
	Pos1 = Pos0 + FVector(-xz, -xz, uzunluk);
	Pos2 = Pos0 + FVector(-xz, -xz, -uzunluk);
	Pos3 = Pos0 + FVector(xz, -xz, -uzunluk);
	Pos4 = Pos0 + FVector(xz, -xz, uzunluk);
	Pos5 = Pos0 + FVector(-xz, xz, uzunluk);
	Pos6 = Pos0 + FVector(-xz, xz, -uzunluk);
	Pos7 = Pos0 + FVector(xz, xz, -uzunluk);
	Pos8 = Pos0 + FVector(xz, xz, uzunluk);
	Pos0 = UE4::SDK::ProjectWorldToScreen(Pos0);
	Pos1 = UE4::SDK::ProjectWorldToScreen(Pos1);
	Pos2 = UE4::SDK::ProjectWorldToScreen(Pos2);
	Pos3 = UE4::SDK::ProjectWorldToScreen(Pos3);
	Pos4 = UE4::SDK::ProjectWorldToScreen(Pos4);
	Pos5 = UE4::SDK::ProjectWorldToScreen(Pos5);
	Pos6 = UE4::SDK::ProjectWorldToScreen(Pos6);
	Pos7 = UE4::SDK::ProjectWorldToScreen(Pos7);
	Pos8 = UE4::SDK::ProjectWorldToScreen(Pos8);
	DrawLine(Pos1.x, Pos1.y, Pos2.x, Pos2.y, ESPColor, Thickness);
	DrawLine(Pos2.x, Pos2.y, Pos3.x, Pos3.y, ESPColor, Thickness);
	DrawLine(Pos3.x, Pos3.y, Pos4.x, Pos4.y, ESPColor, Thickness);
	DrawLine(Pos4.x, Pos4.y, Pos1.x, Pos1.y, ESPColor, Thickness);
	DrawLine(Pos5.x, Pos5.y, Pos6.x, Pos6.y, ESPColor, Thickness);
	DrawLine(Pos6.x, Pos6.y, Pos7.x, Pos7.y, ESPColor, Thickness);
	DrawLine(Pos7.x, Pos7.y, Pos8.x, Pos8.y, ESPColor, Thickness);
	DrawLine(Pos8.x, Pos8.y, Pos5.x, Pos5.y, ESPColor, Thickness);
	DrawLine(Pos1.x, Pos1.y, Pos5.x, Pos5.y, ESPColor, Thickness);
	DrawLine(Pos2.x, Pos2.y, Pos6.x, Pos6.y, ESPColor, Thickness);
	DrawLine(Pos3.x, Pos3.y, Pos7.x, Pos7.y, ESPColor, Thickness);
	DrawLine(Pos4.x, Pos4.y, Pos8.x, Pos8.y, ESPColor, Thickness);
}

void BoxSpike(FVector Root, D3DCOLOR ESPColor, int Thickness)
{
	int xz = 40;
	int uzunluk = 40;
	FVector Pos0, Pos1, Pos2, Pos3, Pos4, Pos5, Pos6, Pos7, Pos8;
	Pos0 = Root;
	Pos0.z = Pos0.z + 40;
	Pos1 = Pos0 + FVector(-xz, -xz, uzunluk);
	Pos2 = Pos0 + FVector(-xz, -xz, -uzunluk);
	Pos3 = Pos0 + FVector(xz, -xz, -uzunluk);
	Pos4 = Pos0 + FVector(xz, -xz, uzunluk);
	Pos5 = Pos0 + FVector(-xz, xz, uzunluk);
	Pos6 = Pos0 + FVector(-xz, xz, -uzunluk);
	Pos7 = Pos0 + FVector(xz, xz, -uzunluk);
	Pos8 = Pos0 + FVector(xz, xz, uzunluk);
	Pos0 = UE4::SDK::ProjectWorldToScreen(Pos0);
	Pos1 = UE4::SDK::ProjectWorldToScreen(Pos1);
	Pos2 = UE4::SDK::ProjectWorldToScreen(Pos2);
	Pos3 = UE4::SDK::ProjectWorldToScreen(Pos3);
	Pos4 = UE4::SDK::ProjectWorldToScreen(Pos4);
	Pos5 = UE4::SDK::ProjectWorldToScreen(Pos5);
	Pos6 = UE4::SDK::ProjectWorldToScreen(Pos6);
	Pos7 = UE4::SDK::ProjectWorldToScreen(Pos7);
	Pos8 = UE4::SDK::ProjectWorldToScreen(Pos8);
	DrawLine(Pos1.x, Pos1.y, Pos2.x, Pos2.y, ESPColor, Thickness);
	DrawLine(Pos2.x, Pos2.y, Pos3.x, Pos3.y, ESPColor, Thickness);
	DrawLine(Pos3.x, Pos3.y, Pos4.x, Pos4.y, ESPColor, Thickness);
	DrawLine(Pos4.x, Pos4.y, Pos1.x, Pos1.y, ESPColor, Thickness);
	DrawLine(Pos5.x, Pos5.y, Pos6.x, Pos6.y, ESPColor, Thickness);
	DrawLine(Pos6.x, Pos6.y, Pos7.x, Pos7.y, ESPColor, Thickness);
	DrawLine(Pos7.x, Pos7.y, Pos8.x, Pos8.y, ESPColor, Thickness);
	DrawLine(Pos8.x, Pos8.y, Pos5.x, Pos5.y, ESPColor, Thickness);
	DrawLine(Pos1.x, Pos1.y, Pos5.x, Pos5.y, ESPColor, Thickness);
	DrawLine(Pos2.x, Pos2.y, Pos6.x, Pos6.y, ESPColor, Thickness);
	DrawLine(Pos3.x, Pos3.y, Pos7.x, Pos7.y, ESPColor, Thickness);
	DrawLine(Pos4.x, Pos4.y, Pos8.x, Pos8.y, ESPColor, Thickness);
}

void Box3D(FVector HeadBone, FVector RootBone, D3DCOLOR ESPColor, int Thickness)
{
	int xz = 50;
	int uzunluk = 105;
	FVector Pos0, Pos1, Pos2, Pos3, Pos4, Pos5, Pos6, Pos7, Pos8;
	Pos0 = GetMidPoint(HeadBone, RootBone);
	Pos0.z = Pos0.z + 5;
	Pos1 = Pos0 + FVector(-xz, -xz, uzunluk);
	Pos2 = Pos0 + FVector(-xz, -xz, -uzunluk);
	Pos3 = Pos0 + FVector(xz, -xz, -uzunluk);
	Pos4 = Pos0 + FVector(xz, -xz, uzunluk);
	Pos5 = Pos0 + FVector(-xz, xz, uzunluk);
	Pos6 = Pos0 + FVector(-xz, xz, -uzunluk);
	Pos7 = Pos0 + FVector(xz, xz, -uzunluk);
	Pos8 = Pos0 + FVector(xz, xz, uzunluk);
	Pos0 = UE4::SDK::ProjectWorldToScreen(Pos0);
	Pos1 = UE4::SDK::ProjectWorldToScreen(Pos1);
	Pos2 = UE4::SDK::ProjectWorldToScreen(Pos2);
	Pos3 = UE4::SDK::ProjectWorldToScreen(Pos3);
	Pos4 = UE4::SDK::ProjectWorldToScreen(Pos4);
	Pos5 = UE4::SDK::ProjectWorldToScreen(Pos5);
	Pos6 = UE4::SDK::ProjectWorldToScreen(Pos6);
	Pos7 = UE4::SDK::ProjectWorldToScreen(Pos7);
	Pos8 = UE4::SDK::ProjectWorldToScreen(Pos8);
	DrawLine(Pos1.x, Pos1.y, Pos2.x, Pos2.y, ESPColor, Thickness);
	DrawLine(Pos2.x, Pos2.y, Pos3.x, Pos3.y, ESPColor, Thickness);
	DrawLine(Pos3.x, Pos3.y, Pos4.x, Pos4.y, ESPColor, Thickness);
	DrawLine(Pos4.x, Pos4.y, Pos1.x, Pos1.y, ESPColor, Thickness);
	DrawLine(Pos5.x, Pos5.y, Pos6.x, Pos6.y, ESPColor, Thickness);
	DrawLine(Pos6.x, Pos6.y, Pos7.x, Pos7.y, ESPColor, Thickness);
	DrawLine(Pos7.x, Pos7.y, Pos8.x, Pos8.y, ESPColor, Thickness);
	DrawLine(Pos8.x, Pos8.y, Pos5.x, Pos5.y, ESPColor, Thickness);
	DrawLine(Pos1.x, Pos1.y, Pos5.x, Pos5.y, ESPColor, Thickness);
	DrawLine(Pos2.x, Pos2.y, Pos6.x, Pos6.y, ESPColor, Thickness);
	DrawLine(Pos3.x, Pos3.y, Pos7.x, Pos7.y, ESPColor, Thickness);
	DrawLine(Pos4.x, Pos4.y, Pos8.x, Pos8.y, ESPColor, Thickness);
}
void drawRecoil() {
	float screen_width = GetSystemMetrics(SM_CXSCREEN);
	float screen_height = GetSystemMetrics(SM_CYSCREEN);
	Vector3 punchAngle = (Camera_Rotation - Control_Rotation).Clamp();
	int centerX = screen_width / 2; // may need to add or subtract to make perfect
	int centerY = screen_height / 2;

	int dx = screen_width / 90;
	int dy = screen_height / 90;

	centerX += (dx * (punchAngle.y));
	centerY -= (dy * (punchAngle.x));

	ImGui::GetForegroundDrawList()->AddCircle(ImVec2(centerX, centerY), 3, ImColor(255, 0, 0), 0, 2);
}

float RadianToDegree(float radian)
{
	return radian * (180 / M_PI);
}

float DegreeToRadian(float degree)
{
	return degree * (M_PI / 180);

}

FVector RadianToDegree(FVector radians)
{
	FVector degrees;
	degrees.x = radians.x * (180 / M_PI);
	degrees.y = radians.y * (180 / M_PI);
	degrees.z = radians.z * (180 / M_PI);
	return degrees;
}

FVector DegreeToRadian(FVector degrees)
{
	FVector radians;
	radians.x = degrees.x * (M_PI / 180);
	radians.y = degrees.y * (M_PI / 180);
	radians.z = degrees.z * (M_PI / 180);
	return radians;
}

FVector getBonePosition(uintptr_t enemy, int index)
{
	size_t size = sizeof(FTransform);
	FTransform firstBone = readv<FTransform>(enemy + (size * index));
	FTransform componentToWorld = readv<FTransform>(enemy + offsets::component_to_world);
	D3DMATRIX matrix = MatrixMultiplication(firstBone.ToMatrixWithScale(), componentToWorld.ToMatrixWithScale());
	return FVector(matrix._41, matrix._42, matrix._43);
}

bool isvisible(uintptr_t mesh)
{
	float fLastSubmitTime = readv<float>(mesh + offsets::last_render_time);
	float fLastRenderTimeOnScreen = readv<float>(mesh + offsets::last_submit_time);
	const float fVisionTick = 0.06f;
	return fLastRenderTimeOnScreen + fVisionTick >= fLastSubmitTime;
}


std::string get_key_name_by_id(int id);

// ~ ImGui widget for keybind button
void keybind_button(int& i_key, int i_width, int i_height)
{
	static auto b_get = false;
	static std::string sz_text(skCrypt("Click to bind"));

	if (ImGui::Button(sz_text.c_str(), ImVec2(static_cast<float>(i_width), static_cast<float>(i_height))))
		b_get = true;

	if (b_get)
	{
		for (auto i = 1; i < 256; i++)
		{
			if (GetAsyncKeyState(i) & 0x8000)
			{
				if (i != 12)
				{
					i_key = i == VK_ESCAPE ? 0 : i;
					b_get = false;
				}
			}
		}
		sz_text = std::string(skCrypt("Press a key"));
	}
	else if (!b_get && i_key == 0)
		sz_text = std::string(skCrypt("Click to bind"));
	else if (!b_get && i_key != 0)
		sz_text = std::string(skCrypt("Selected ")) + get_key_name_by_id(i_key);
}

// ~ returns a key name from a keycode
std::string get_key_name_by_id(int id)
{
	static std::unordered_map<int, std::string> key_names = {
	{ 0, std::string(skCrypt("None")) },
	{ VK_LBUTTON, std::string(skCrypt("Mouse 1")) },
	{ VK_RBUTTON, std::string(skCrypt("Mouse 2")) },
	{ VK_MBUTTON, std::string(skCrypt("Mouse 3")) },
	{ VK_XBUTTON1, std::string(skCrypt("Mouse 4")) },
	{ VK_XBUTTON2, std::string(skCrypt("Mouse 5")) },
	{ VK_BACK, std::string(skCrypt("Back")) },
	{ VK_TAB, std::string(skCrypt("Tab")) },
	{ VK_CLEAR, std::string(skCrypt("Clear")) },
	{ VK_RETURN, std::string(skCrypt("Enter")) },
	{ VK_SHIFT, std::string(skCrypt("Shift")) },
	{ VK_CONTROL, std::string(skCrypt("Ctrl")) },
	{ VK_MENU, std::string(skCrypt("Alt")) },
	{ VK_PAUSE, std::string(skCrypt("Pause")) },
	{ VK_CAPITAL, std::string(skCrypt("Caps Lock")) },
	{ VK_ESCAPE, std::string(skCrypt("Escape")) },
	{ VK_SPACE, std::string(skCrypt("Space")) },
	{ VK_PRIOR, std::string(skCrypt("Page Up")) },
	{ VK_NEXT, std::string(skCrypt("Page Down")) },
	{ VK_END, std::string(skCrypt("End")) },
	{ VK_HOME, std::string(skCrypt("Home")) },
	{ VK_LEFT, std::string(skCrypt("Left Key")) },
	{ VK_UP, std::string(skCrypt("Up Key")) },
	{ VK_RIGHT, std::string(skCrypt("Right Key")) },
	{ VK_DOWN, std::string(skCrypt("Down Key")) },
	{ VK_SELECT, std::string(skCrypt("Select")) },
	{ VK_PRINT, std::string(skCrypt("Print Screen")) },
	{ VK_INSERT, std::string(skCrypt("Insert")) },
	{ VK_DELETE, std::string(skCrypt("Delete")) },
	{ VK_HELP, std::string(skCrypt("Help")) },
	{ VK_SLEEP, std::string(skCrypt("Sleep")) },
	{ VK_MULTIPLY, std::string(skCrypt("*")) },
	{ VK_ADD, std::string(skCrypt("+")) },
	{ VK_SUBTRACT, std::string(skCrypt("-")) },
	{ VK_DECIMAL, std::string(skCrypt(".")) },
	{ VK_DIVIDE, std::string(skCrypt("/")) },
	{ VK_NUMLOCK, std::string(skCrypt("Num Lock")) },
	{ VK_SCROLL, std::string(skCrypt("Scroll")) },
	{ VK_LSHIFT, std::string(skCrypt("Left Shift")) },
	{ VK_RSHIFT, std::string(skCrypt("Right Shift")) },
	{ VK_LCONTROL, std::string(skCrypt("Left Ctrl")) },
	{ VK_RCONTROL, std::string(skCrypt("Right Ctrl")) },
	{ VK_LMENU, std::string(skCrypt("Left Alt")) },
	{ VK_RMENU, std::string(skCrypt("Right Alt")) },
	};

	if (id >= 0x30 && id <= 0x5A)
		return std::string(1, (char)id);

	if (id >= 0x60 && id <= 0x69)
		return std::string(skCrypt("Num ")) + std::to_string(id - 0x60);

	if (id >= 0x70 && id <= 0x87)
		return std::string(skCrypt("F")) + std::to_string((id - 0x70) + 1);

	return key_names[id];
}


float GetDistance(FVector agka)
{
	return sqrt((agka.y - ScreenCenterY) * (agka.y - ScreenCenterY) + (agka.x - ScreenCenterX) * (agka.x - ScreenCenterX));
}

void DrawCircleFilled2(int x, int y, int radius, D3DCOLOR color)
{
	ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(x, y), radius, color);
}
bool AimSortCrossHair(const Target& pf1, const Target& pf2)
{
	return pf1.fovDist <= pf2.fovDist;
}

float CalculateDistance(int p1x, int p1y, int p2x, int p2y)
{
	float diffY = p1y - p2y;
	float diffX = p1x - p2x;
	return sqrt((diffY * diffY) + (diffX * diffX));
}

D3DXVECTOR2 WorldRadar(FVector srcPos, FVector distPos, float yaw, float radarX, float radarY, float size)
{
	auto cosYaw = cos(DegreeToRadian(-yaw));
	auto sinYaw = sin(DegreeToRadian(-yaw));

	auto deltaX = srcPos.x - distPos.x;
	auto deltaY = srcPos.y - distPos.y;

	auto locationX = (float)(deltaY * cosYaw + deltaX * sinYaw) / 30;
	auto locationY = (float)(deltaX * cosYaw - deltaY * sinYaw) / 30;

	if (locationX > size - 2.f)
		locationX = size - 2.f;
	else if (locationX < -(size - 2.f))
		locationX = -(size - 2.f);

	if (locationY > size - 12.f)
		locationY = size - 12.f;
	else
		if (locationY < -(size - 12.f))
			locationY = -(size - 12.f);

	return D3DXVECTOR2((int)(-locationX + radarX), (int)(locationY + radarY));
}
void DrawRadarHUD(int xAxis, int yAxis, int width, int height)
{
	bool out = false;
	D3DXVECTOR3 siz;
	siz.x = width;
	siz.y = height;
	float RadarCenterX = siz.x / 2;
	float RadarCenterY = siz.y / 2;
	DrawCircleFilled2(xAxis, yAxis, 4, D3DCOLOR_XRGB(255, 255, 255));

	DrawOutlinedBox2(FVector(xAxis, yAxis, 0), siz.x, siz.y, D3DCOLOR_XRGB(255, 255, 255));
	DrawFilledRect2(xAxis - RadarCenterX, yAxis - RadarCenterY, siz.x, siz.y, D3DCOLOR_ARGB(20, 0, 0, 0));
	DrawLine(xAxis, yAxis, xAxis - RadarCenterX, yAxis - RadarCenterY, D3DCOLOR_XRGB(0, 0, 0), 1);
	DrawLine(xAxis, yAxis, xAxis + RadarCenterX, yAxis - RadarCenterY, D3DCOLOR_XRGB(0, 0, 0), 1);
	DrawLine(xAxis - RadarCenterX, yAxis, xAxis + RadarCenterX, yAxis, D3DCOLOR_XRGB(0, 0, 0), 1);
	DrawLine(xAxis, yAxis, xAxis, yAxis + RadarCenterY, D3DCOLOR_XRGB(0, 0, 0), 1);

}
void DrawRadar(ImVec2 pos, FVector LocalPos, FVector EntityPos, D3DCOLOR EntityColor)
{
	auto radar_posX = pos.x;
	auto radar_posY = pos.y;
	FRotator camrot = APawn->RelativeRotation(LocalPlayer);
	auto Radar2D = WorldRadar(LocalPos, EntityPos, camrot.Yaw, radar_posX, radar_posY, 100.f);// radar pos
	ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(Radar2D.x, Radar2D.y), 2, EntityColor, 0);
}
float espcolor[] = { 1.00f,0.00f,0.00f,1.00f };
float espcolor2[] = { 0.00f,1.00f,0.00f,1.00f };
float headcolor[] = { 1.00f,1.00f,1.00f,1.00f };
float espline[] = { 1.00f,1.00f,1.00f,1.00f };
float skeletoncolor[] = { 1.00f,1.00f,1.00f,1.00f };
float itemcolor[] = { 1.00f,1.00f,1.00f,1.00f };
int selectedLine = 0;
int selectedKey = 0;

int selectedColor = 0;
int selectedBox = 0;


int iTab = 0;
int AimFOV = 30;
float smooth = 2;


uintptr_t GetModuleBaseAddress(DWORD procId, const char* modName)
{
	uintptr_t modBaseAddr = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry))
		{
			do
			{
				if (!_stricmp(modEntry.szModule, modName))
				{
					modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
					break;
				}
			} while (Module32Next(hSnap, &modEntry));
		}
	}
	CloseHandle(hSnap);
	return modBaseAddr;
}
char* wchar_to_char(const wchar_t* pwchar)
{
	int currentCharIndex = 0;
	char currentChar = pwchar[currentCharIndex];

	while (currentChar != '\0')
	{
		currentCharIndex++;
		currentChar = pwchar[currentCharIndex];
	}

	const int charCount = currentCharIndex + 1;

	char* filePathC = (char*)malloc(sizeof(char) * charCount);

	for (int i = 0; i < charCount; i++)
	{
		char character = pwchar[i];

		*filePathC = character;

		filePathC += sizeof(char);

	}
	filePathC += '\0';

	filePathC -= (sizeof(char) * charCount);

	return filePathC;
}

FVector LABNMTRX(int index, ValEntity player)
{
	size_t size = sizeof(FTransform);
	FTransform first_bone, comp_to_world;

	first_bone = readv<FTransform>(player.bone_array + (0x30 * index));
	comp_to_world = readv<FTransform>(player.mesh + 0x250);

	D3DMATRIX matrix = LAmth::MatrixMultiplication(first_bone.ToMatrixWithScale(), comp_to_world.ToMatrixWithScale());
	return FVector(matrix._41, matrix._42, matrix._43);
}


static int Spin = 0;
auto CheatLoop() -> void
{
	int closestplayer = 1337;
	float closest_distance = FLT_MAX;

	D3DCOLOR ESPColor = FLOAT4TOD3DCOLOR(espcolor);
	D3DCOLOR ESPColor2 = FLOAT4TOD3DCOLOR(espcolor2);
	D3DCOLOR SkeletonColor = FLOAT4TOD3DCOLOR(skeletoncolor);
	D3DCOLOR HeadColor = FLOAT4TOD3DCOLOR(headcolor);
	D3DCOLOR EspLineColor = FLOAT4TOD3DCOLOR(espline);
	D3DCOLOR ItemColor = FLOAT4TOD3DCOLOR(itemcolor);
	Control_Rotation = readv<Vector3>(Globals::PlayerController + offsets::control_rotation);
	auto ViewInfo = readv<FMinimalViewInfo>(PlayerCameraManager + 0x1FB0);

	Camera_Rotation = { ViewInfo.Rotation.x,ViewInfo.Rotation.y,ViewInfo.Rotation.z };
	CameraPosition = { ViewInfo.Location.x,ViewInfo.Location.y,ViewInfo.Location.z };

	if (Settings::Visuals::bRadar)
	{
		DrawRadarHUD(Width * 0.9, Height * 0.15, 200, 200);
	}

	for (ValEntity2 ValEntityList2 : ValList2)
	{
		int asdfga = readv<int>(ValEntityList2.Actor + 0x18);
		std::string agent_name = get_fname(asdfga);
		auto RelativeLocation = APawn->RelativeLocation(ValEntityList2.Actor);
		auto RelativeLocationsw = APawn->RelativeLocation(LocalPlayer);
		auto RelativeLocationProjected = UE4::SDK::ProjectWorldToScreen(RelativeLocation);

		if (strstr(agent_name.c_str(), skCrypt("EquippableGroundPickup_C"))) {
			//auto RelativeLocation = APawn->RelativeLocation(ValEntityList2.Actor);
			//auto RelativeLocationProjected = UE4::SDK::ProjectWorldToScreen(RelativeLocation);
			if (Settings::Visuals::bItem)
			{
				BoxCac({ RelativeLocation.x,RelativeLocation.y,RelativeLocation.z }, ItemColor, 10, 80, 1);
			}
		}
		else if (strstr(agent_name.c_str(), skCrypt("TimedBomb_C")))
		{
			float timer = readv<float>(ValEntityList2.Actor + 0x4e4);
			if (Settings::Visuals::bTimer)
				if (timer > 0)
				{
					BombSiteEnum site = readv<BombSiteEnum>(ValEntityList2.Actor + 0x4ec);
					float defuse = readv<float>(ValEntityList2.Actor + 0x510);
					float DefusePercentage = defuse * 100 / 6.984602;
					std::string plantedside = std::string(skCrypt("NULL"));
					if (site == BombSiteEnum::NewEnumerator0)
						plantedside = std::string(skCrypt("A"));
					if (site == BombSiteEnum::NewEnumerator1)
						plantedside = std::string(skCrypt("B"));
					if (site == BombSiteEnum::NewEnumerator2)
						plantedside = std::string(skCrypt("C"));
					ImGui::GetForegroundDrawList()->AddRectFilled({ 10,10 }, { 350,44 }, ImColor(200, 200, 40, 255));
					std::string c_imer = std::string(skCrypt("Spike [")) + std::to_string(timer) +std::string(skCrypt("s] | Defuse: %")) +std::to_string((int)DefusePercentage) + std::string(skCrypt(" | Planted on: ")) + plantedside;
					ImGui::GetForegroundDrawList()->AddText(ImVec2(20, 20), D3DCOLOR_XRGB(0, 0, 0), c_imer.c_str());
					auto RelativeSpike = APawn->RelativeLocation(ValEntityList2.Actor);
					BoxSpike(RelativeSpike, ImColor(255, 255, 255, 255), 1);

				}
		}
	}
	for (int x = 0; x < ValList.size(); x++)
	{
		ValEntity ValEntityList = ValList[x];

		auto SkeletalMesh = APrivatePawn->USkeletalMeshComponent(ValEntityList.Actor);
		auto Health = APawn->Health(ValEntityList.Actor);

		float Fov = ViewInfo.FOV;

		FVector head = LABNMTRX(8, ValEntityList);
		FVector head_w2s = UE4::SDK::ProjectWorldToScreen(head);

		auto RelativeLocation = APawn->RelativeLocation(ValEntityList.Actor);
		auto RelativeLocationLocal = APawn->RelativeLocation(LocalPlayer);
		auto RelativeLocationProjected = UE4::SDK::ProjectWorldToScreen(RelativeLocation);


		auto RelativePosition = RelativeLocation - CameraLocation;
		auto RelativeDistance = RelativePosition.Length() / 10000 * 2;

		auto HeadBone = FVector(RelativeLocation.x, RelativeLocation.y, RelativeLocation.z +83);
		auto HeadBoneProjected = UE4::SDK::ProjectWorldToScreen(HeadBone);
		auto RootBone = FVector(RelativeLocation.x, RelativeLocation.y, RelativeLocation.z - 100);
		auto RootBoneProjected = UE4::SDK::ProjectWorldToScreen(RootBone);
		auto RootBoneProjected2 = UE4::SDK::ProjectWorldToScreen(FVector(RootBone.x, RootBone.y, RootBone.z - 15));
		auto weaponProjected = UE4::SDK::ProjectWorldToScreen(FVector(RootBone.x, RootBone.y, RootBone.z - 55));
		auto namewProjected = UE4::SDK::ProjectWorldToScreen(FVector(HeadBone.x, HeadBone.y, HeadBone.z + 60));
		float Distance = MyRelativeLocation.Distance(RelativeLocation) * 0.01F;
		float BoxHeight = abs(HeadBoneProjected.y - RootBoneProjected.y);
		float BoxWidth = BoxHeight * 0.50;

		auto bone_count = USkeletalMeshComponent->BoneCount(SkeletalMesh);
		bool is_bot = bone_count == 103;

		int TeamID = APawn->TeamID(ValEntityList.Actor);
		bool teamcheck = TeamID == MyTeamID;

		if (!Settings::Visuals::bDeathmatch)
		{
			if (teamcheck && !is_bot) {
				continue;
			}
		}
	    if (Health.hp <= 0) continue;

		auto dormant = readv<int>(ValEntityList.Actor + offsets::dormant);
		float last_render_time = readv<float>(SkeletalMesh + offsets::last_render_time);
		float last_submit_time = readv<float>(SkeletalMesh + offsets::last_submit_time);
		const float fVisionTick = 0.06f;
		bool vischeck = last_render_time + fVisionTick >= last_submit_time;


		uintptr_t invcentoty = readv<uintptr_t>(ValEntityList.Actor + offsets::inventory);
		uintptr_t silahim = readv<uintptr_t>(invcentoty + offsets::current_equippable);
		std::string agent_name = std::string(skCrypt(""));
		std::string weapon_name = std::string(skCrypt(""));
		if (Settings::Visuals::bWepName || Settings::Visuals::bName)
		{
			agent_name = karakterismi(get_fname(readv<int>(ValEntityList.Actor + offsets::actor_id)));
			weapon_name = weaponismi(get_fname(readv<int>(silahim + offsets::actor_id)));
		}
		std::string weapon_string = std::string(skCrypt("Idle"));
		EAresEquippableState weapon_state = readv<EAresEquippableState>(silahim + 0xcf8);
		if (weapon_state == EAresEquippableState::Reloading)
		{
			weapon_string = std::string(skCrypt("Reloading..."));
		}
		if (weapon_state == EAresEquippableState::Idle)
		{
			weapon_string = std::string(skCrypt("Idle"));
		}
		if (weapon_state == EAresEquippableState::Reloading)
		{
			weapon_string = std::string(skCrypt("Reloading..."));
		}
		if (weapon_state == EAresEquippableState::Firing)
		{
			weapon_string = std::string(skCrypt("Firing..."));
		}
		if (weapon_state == EAresEquippableState::Equipping)
		{
			weapon_string = std::string(skCrypt("Equipping..."));
		}

		if (dormant != 1)
		{
			
			if (Settings::Visuals::bSnaplines)
			{
				if (selectedLine == 0 || selectedLine == 1)
					DrawTracers(HeadBoneProjected, EspLineColor);
				if (selectedLine == 2)
					DrawTracers(RootBoneProjected, EspLineColor);

			}
			if (Settings::Visuals::bBox)
			{
				if (selectedBox == 0)
				{
					if (vischeck)
						DrawOutlinedBox(RelativeLocationProjected, BoxWidth, BoxHeight, ESPColor2);
					else
					{
						DrawOutlinedBox(RelativeLocationProjected, BoxWidth, BoxHeight, ESPColor);
					}
				}
				else if (selectedBox == 1)
				{
					if (vischeck)
						Draw2DBox(RelativeLocationProjected, BoxWidth, BoxHeight, ESPColor2);
					else
					{
						Draw2DBox(RelativeLocationProjected, BoxWidth, BoxHeight, ESPColor);
					}
				}
				else if (selectedBox == 2)
				{
					if (vischeck)
						Box3D(HeadBone, RootBone, ESPColor2, 1);
					else
					{
						Box3D(HeadBone, RootBone, ESPColor, 1);
					}
				}
			}


			if (Settings::Visuals::bHealth)
				DrawHealthBar(RelativeLocationProjected, BoxWidth, BoxHeight, Health.hp, RelativeDistance, Health.hp);

			if (Settings::Visuals::bDistance && !Settings::Visuals::bName)
				DrawDistance(weaponProjected, Distance);

			if (Settings::Visuals::bName)
			{
				char dist[64];
				sprintf_s(dist, skCrypt("[%.fm]"), Distance);
				if (Settings::Visuals::bDistance)
				{
					DrawPNL(namewProjected, Distance, std::string(dist + std::string(skCrypt(" ")) + agent_name).c_str());
				}
				else
				{
					DrawPNL(namewProjected, Distance, std::string(agent_name).c_str());
				}
			}
			if (Settings::Visuals::bWepName)
			{
				char dist[64];
				sprintf_s(dist, skCrypt("[%.fm]"), Distance);
				DrawWPN(weaponProjected, std::string(weapon_name + std::string(skCrypt(" ")) + std::string(weapon_string).c_str()));
			}

			if (Settings::Visuals::bSkeleton)
				renderBones(SkeletalMesh, SkeletonColor);

			if (Settings::Visuals::bRadar)
				DrawRadar(ImVec2(Width * 0.9, Height * 0.15 ), RelativeLocationLocal, RelativeLocation,D3DCOLOR_XRGB(255,0,0));

			if (Settings::Visuals::bHeadBox)
				ImGui::GetForegroundDrawList()->AddCircleFilled({HeadBoneProjected.x, HeadBoneProjected.y}, 50 / Distance, HeadColor, 0);

			if (Settings::Visuals::bDirectline)
			{
				FRotator camrot = APawn->RelativeRotation(ValEntityList.Actor);
				int DirectionLineSize = 200;
				FVector start = HeadBone;
				FVector angles = camrot.ToVector();
				FVector end = angles * DirectionLineSize + start;
				FVector screen_start, screen_end;

				screen_start = UE4::SDK::ProjectWorldToScreen(start);
				screen_end = UE4::SDK::ProjectWorldToScreen(end);
				DrawLine(screen_start.x, screen_start.y, screen_end.x, screen_end.y, D3DCOLOR_XRGB(255, 255, 255), 2);
				BoxCac(end, D3DCOLOR_XRGB(0, 255, 0), 10, 30, 1);
			}

			if (Settings::Visuals::bTrigger)
			{
				int bonecount = readv<int>(SkeletalMesh + offsets::bone_count);
				for (int b = 1; b < bonecount; b++) {

					FVector bonePos = UE4::SDK::GetEntityBone(SkeletalMesh, b);
					FVector bonePos_w2s = UE4::SDK::ProjectWorldToScreen(bonePos);
					FVector bonePos_w2s_relative;
					bonePos_w2s_relative.x = bonePos_w2s.x - ScreenCenterX;
					bonePos_w2s_relative.y = bonePos_w2s.y - ScreenCenterY;

					float screen_width = GetSystemMetrics(SM_CXSCREEN);
					float screen_height = GetSystemMetrics(SM_CYSCREEN);
					Vector3 punchAngle = (Camera_Rotation - Control_Rotation).Clamp();
					int centerX = screen_width / 2;
					int centerY = screen_height / 2;

					int dx = screen_width / 90;
					int dy = screen_height / 90;

					centerX += (dx * (punchAngle.y));
					centerY -= (dy * (punchAngle.x));
					double dist = sqrt(pow((centerX)-bonePos_w2s.x, 2) + pow((centerY)-bonePos_w2s.y, 2));


					if (abs(dist) < 4)
					{
						mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
						mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
					}
				}
			}

			FVector head = LABNMTRX(8, ValEntityList);
			FVector head_w2s = UE4::SDK::ProjectWorldToScreen(head);

			float delta_x = head_w2s.x - (Width / 2.f);
			float delta_y = head_w2s.y - (Height / 2.f);
			float dist = sqrtf(delta_x * delta_x + delta_y * delta_y);
			float fovdist = CalculateDistance(Width / 2, Height / 2, head_w2s.x, head_w2s.y);
			if ((dist < closest_distance) && fovdist < Settings::Visuals::FovValue) {
				closest_distance = dist;
				closestplayer = x;
			}
		}

	}

	if (Settings::Visuals::Aimbott && !Settings::Visuals::RCSOpen && closestplayer != 1337)
	{
		FVector bone;

		ValEntity this_player = ValList[closestplayer];

		FVector head = LABNMTRX(8, this_player);
		FVector chest = LABNMTRX(6, this_player);
		FVector pelvis = LABNMTRX(3, this_player);

		if (Settings::Visuals::boneselect == 0)
		{
			bone = head;
		}

		if (Settings::Visuals::boneselect == 1)
		{
			bone = chest;
		}

		if (Settings::Visuals::boneselect == 2)
		{
			bone = pelvis;
		}

		FVector rootpos = readv<FVector>(this_player.root_component + 0x164);

		if (bone.z <= rootpos.z)
		{
			return;
		}

		FVector localView = readv<FVector>(PlayerController + 0x448);
		FVector vecCaclculatedAngles = fhgfsdhkfshdghfsd205(Camera::CameraLocation, bone);
		FVector angleEx = CaadadalcAngle(Camera::CameraLocation, bone);
		FVector fin = FVector(vecCaclculatedAngles.y, angleEx.y, 0);
		FVector delta = fin - localView;
		NormalizeAngles(delta);
		FVector TargetAngle = localView + (delta / Settings::Visuals::SmoothValue);
		NormalizeAngles(TargetAngle);

		int AimKeyList[] = { VK_LBUTTON , VK_RBUTTON , VK_MBUTTON , VK_MENU , VK_CONTROL , VK_SHIFT , VK_XBUTTON1, VK_XBUTTON2 };

		if (GetAsyncKeyState(AimKeyList[Settings::Visuals::keyselect]) & 0x8000)
		{
			write<FVector>(PlayerController + 0x448, TargetAngle);

		}
	}

	if (Settings::Visuals::Aimbott && Settings::Visuals::RCSOpen && closestplayer != 1337)
	{
		FVector bone;

		ValEntity this_player = ValList[closestplayer];

		FVector head = LABNMTRX(8, this_player);
		FVector chest = LABNMTRX(6, this_player);
		FVector pelvis = LABNMTRX(3, this_player);

		if (Settings::Visuals::boneselect == 0)
		{
			bone = head;
		}

		if (Settings::Visuals::boneselect == 1)
		{
			bone = chest;
		}

		if (Settings::Visuals::boneselect == 2)
		{
			bone = pelvis;
		}

		FVector rootpos = readv<FVector>(this_player.root_component + 0x164);

		if (bone.z <= rootpos.z)
		{
			return;
		}

		FVector localView = readv<FVector>(PlayerController + 0x448);
		FVector vecCaclculatedAngles = fhgfsdhkfshdghfsd205(Camera::CameraLocation, bone);
		FVector angleEx = CaadadalcAngle(Camera::CameraLocation, bone);
		FVector fin = FVector(vecCaclculatedAngles.y, angleEx.y, 0);
		FVector delta = fin - localView;
		FVector TargetAngle2 = localView + delta;
		Clamp(TargetAngle2);

		int AimKeyList[] = { VK_LBUTTON , VK_RBUTTON , VK_MBUTTON , VK_MENU , VK_CONTROL , VK_SHIFT , VK_XBUTTON1, VK_XBUTTON2 };

		if (GetAsyncKeyState(AimKeyList[Settings::Visuals::keyselect]) & 0x8000)
		{
			RCS(TargetAngle2, Camera::CameraLocation, Settings::Visuals::SmoothValue);

		}

	}

	if (Settings::Visuals::bCross)
		drawRecoil();
	if (Settings::Visuals::bFov)
		ImGui::GetForegroundDrawList()->AddCircle(ImVec2(ScreenCenterX, ScreenCenterY), AimFOV * 2, ImColor(255, 255, 255), 0, 2);

}