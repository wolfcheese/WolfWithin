#include "../tomb3/pch.h"
#include "sub.h"
#include "../specific/init.h"
#include "control.h"
#include "collide.h"
#include "laraflar.h"
#include "objects.h"
#include "draw.h"
#include "../specific/output.h"
#include "../3dsystem/3d_gen.h"
#include "../specific/draweffects.h"
#include "../3dsystem/phd_math.h"
#include "lara.h"
#include "laraswim.h"
#include "sound.h"
#include "camera.h"
#include "../specific/game.h"
#include "items.h"
#include "sphere.h"
#include "effect2.h"
#include "effects.h"
#include "../specific/input.h"
#include "savegame.h"
#include "../newstuff/LaraDraw.h"
#include "larafire.h"
#include "laramisc.h"

BITE_INFO sub_bites[6] =
{
	{ 0, 0, 0, 3 },
	{ 0, 96, 256, 0 },
	{ -128, 0, -64, 1 },
	{ 0, 0, -64, 1 },
	{ 128, 0, -64, 2 },
	{ 0, 0, -64, 2 }
};

SUB_WAKE_PTS SubWakePts[32][2];
uchar SubWakeShade;
uchar SubCurrentStartWake;

void SubInitialise(short item_number)
{
	ITEM_INFO* item;
	SUBINFO* sub;

	item = &items[item_number];
	sub = (SUBINFO*)game_malloc(sizeof(SUBINFO));
	item->data = sub;
	sub->Rot = 0;
	sub->Vel = 0;
	sub->Flags = 2;
	sub->WeaponTimer = 0;

	for (int i = 0; i < 32; i++)
	{
		SubWakePts[i][0].life = 0;
		SubWakePts[i][1].life = 0;
	}
}

static long GetOnSub(short item_number, COLL_INFO* coll)
{
	ITEM_INFO* item;
	FLOOR_INFO* floor;
	long dx, dy, dz, dist, h;
	short room_number;

	if (!(input & IN_ACTION) || lara.gun_status != LG_ARMLESS || lara_item->gravity_status)
		return 0;

	item = &items[item_number];
	dy = lara_item->pos.y_pos - item->pos.y_pos + 128;

	if (abs(dy) > 256)
		return 0;

	dx = lara_item->pos.x_pos - item->pos.x_pos;
	dz = lara_item->pos.z_pos - item->pos.z_pos;
	dist = SQUARE(dx) + SQUARE(dz);

	if (dist > 0x40000)
		return 0;

	room_number = item->room_number;
	floor = GetFloor(item->pos.x_pos, item->pos.y_pos, item->pos.z_pos, &room_number);
	h = GetHeight(floor, item->pos.x_pos, item->pos.y_pos, item->pos.z_pos);

	if (h < -32000)
		return 0;

	return 1;
}

void SubCollision(short item_number, ITEM_INFO* l, COLL_INFO* coll)
{
	ITEM_INFO* item;

	item = &items[item_number];

	if (l->hit_points < 0 || lara.skidoo != NO_ITEM)
		return;

	if (GetOnSub(item_number, coll))
	{
		//wc - trigger upv
		lara.skidoo = item_number;
		item->status = ITEM_ACTIVE;
		item->flags |= IFL_CODEBITS;
		AddActiveItem(item_number);
		lara.water_status = LARA_ABOVEWATER;

		//wc - make sure sub doesn't go crazy right away
		lara.current_xvel = 0;
		lara.current_zvel = 0;
		lara.current_active = 0;

		if (lara.gun_type == LG_FLARE)
		{
			CreateFlare(0);
			undraw_flare_meshes();
			lara.flare_control_left = 0;
			lara.gun_type = LG_UNARMED;
			lara.request_gun_type = LG_UNARMED;
		}

		lara.gun_status = 1;
		item->hit_points = 1;
		l->pos.x_pos = item->pos.x_pos;
		l->pos.y_pos = item->pos.y_pos;
		l->pos.z_pos = item->pos.z_pos;
		l->pos.y_rot = item->pos.y_rot;

		if (l->current_anim_state == AS_SURFTREAD || l->current_anim_state == AS_SURFSWIM)
		{
			l->anim_number = objects[VEHICLE_ANIM].anim_index + 10;
			l->frame_number = anims[objects[UPV].anim_index + 10].frame_base;
		}
		else
		{
			l->anim_number = objects[VEHICLE_ANIM].anim_index + 13;
			l->frame_number = anims[objects[UPV].anim_index + 13].frame_base;
		}

		l->goal_anim_state = 8;
		l->current_anim_state = 8;
		AnimateItem(l);
	}
	else
	{
		item->pos.y_pos += 128;
		ObjectCollisionSub(item_number, l, coll);
		item->pos.y_pos -= 128;
	}
}

static void SubBaddieCollision(ITEM_INFO* sub)
{
	ITEM_INFO* item;
	OBJECT_INFO* obj;
	short* doors;
	long dx, dy, dz;
	short roomies[16];
	short room_count, item_number;

	room_count = 1;
	roomies[0] = sub->room_number;
	doors = room[sub->room_number].door;

	if (doors)
	{
		for (int i = *doors++; i > 0; i--, doors += 16)
		{
			roomies[room_count] = *doors;
			room_count++;
		}
	}

	for (int i = 0; i < room_count; i++)
	{
		for (item_number = room[roomies[i]].item_number; item_number != NO_ITEM; item_number = item->next_item)
		{
			item = &items[item_number];

			if (item->collidable && item->status != ITEM_INVISIBLE && item != lara_item && item != sub)
			{
				obj = &objects[item->object_number];

				//wc - smash wall if the upv collides with it
				if (obj->collision && item->object_number == SMASH_OBJECT1)
				{
					dx = sub->pos.x_pos - item->pos.x_pos;
					dy = sub->pos.y_pos - item->pos.y_pos;
					dz = sub->pos.z_pos - item->pos.z_pos;

					if (dx > -2248 && dx < 2248 && dz > -2248 && dz < 2248 && dy > -2248 && dy < 2248 && TestBoundsCollide(item, sub, 500))
					{
						SoundEffect(SFX_LARA_HARPOON_FIRE_WATER, &lara_item->pos, SFX_ALWAYS);
						SmashItem(item_number, 0);
					}
				}
				//wc - traps damage
				else if(obj->collision && (item->object_number == DARTS || item->object_number == CEILING_SPIKES || item->object_number == SWING_BOX || (item->object_number == BLADE && item->current_anim_state != 1)))
				{
					dx = sub->pos.x_pos - item->pos.x_pos;
					dy = sub->pos.y_pos - item->pos.y_pos;
					dz = sub->pos.z_pos - item->pos.z_pos;

					if (dx > -2048 && dx < 2048 && dz > -2048 && dz < 2048 && dy > -2048 && dy < 2048)
					{
						if (TestBoundsCollide(item, sub, 256))
						{
							DoLotsOfBlood(lara_item->pos.x_pos, lara_item->pos.y_pos - 256, lara_item->pos.z_pos,
								sub->speed, sub->pos.y_rot, lara_item->room_number, 3);
							lara_item->hit_points -= 50;
						}
					}
				}
				else if (obj->collision && item->object_number == TRAIN)
				{
					dx = sub->pos.x_pos - item->pos.x_pos;
					dy = sub->pos.y_pos - item->pos.y_pos;
					dz = sub->pos.z_pos - item->pos.z_pos;

					if (dx > -2048 && dx < 2048 && dz > -2048 && dz < 2048 && dy > -2048 && dy < 2048)
					{
						if (TestBoundsCollide(item, sub, 256))
						{
							DoLotsOfBlood(lara_item->pos.x_pos, lara_item->pos.y_pos - 256, lara_item->pos.z_pos,
								sub->speed, sub->pos.y_rot, lara_item->room_number, 3);
							lara_item->hit_points = 0;
							item->item_flags[1] -= 48;
						}
					}
				}
				else //wc - jellyfish electric death
					if (obj->collision && CurrentLevel == 2 && item->object_number == ANIMATING5)
					{
						dx = sub->pos.x_pos - item->pos.x_pos;
						dy = sub->pos.y_pos - item->pos.y_pos;
						dz = sub->pos.z_pos - item->pos.z_pos;

						if (dx > -2048 && dx < 2048 && dz > -2048 && dz < 2048 && dy > -2048 && dy < 2048)
						{
							if (TestBoundsCollide(item, sub, 256))
							{
								lara.electric = 1;
								lara_item->hit_points = 0;
							}
						}
					}
			}
		}
	}
}

void SubDraw(ITEM_INFO* item)
{
	OBJECT_INFO* obj;
	SUBINFO* sub;
	short** meshpp;
	long* bone;
	short* frm[2];
	short* rot1;
	short* rot2;
	long frac, rate, clip;

	frac = GetFrames(item, frm, &rate);
	obj = &objects[item->object_number];
	S_PrintShadow(256, frm[0], item);

	phd_PushMatrix();
	phd_TranslateAbs(item->pos.x_pos, item->pos.y_pos + 128, item->pos.z_pos);
	phd_RotYXZ(item->pos.y_rot, item->pos.x_rot, item->pos.z_rot);

	clip = S_GetObjectBounds(frm[0]);

	if (clip)
	{
		sub = (SUBINFO*)item->data;
		CalculateObjectLighting(item, frm[0]);
		meshpp = &meshes[obj->mesh_index];
		bone = &bones[obj->bone_index];

		if (frac)
		{
			InitInterpolate(frac, rate);
			rot1 = frm[0] + 9;
			rot2 = frm[1] + 9;
			phd_TranslateRel_ID(frm[0][6], frm[0][7], frm[0][8], frm[1][6], frm[1][7], frm[1][8]);

			gar_RotYXZsuperpack_I(&rot1, &rot2, 0);
			phd_PutPolygons_I(*meshpp++, clip);

			phd_PushMatrix_I();
			phd_TranslateRel_I(bone[1], bone[2], bone[3]);
			gar_RotYXZsuperpack_I(&rot1, &rot2, 0);
			phd_RotX_I(short(item->pos.z_rot + (sub->RotX >> 13)));
			InterpolateMatrix();
			phd_PutPolygons_I(*meshpp++, clip);
			phd_PopMatrix_I();

			phd_PushMatrix_I();
			phd_TranslateRel_I(bone[5], bone[6], bone[7]);
			gar_RotYXZsuperpack_I(&rot1, &rot2, 0);
			phd_RotX_I(short((sub->RotX >> 13) - item->pos.z_rot));
			phd_PutPolygons_I(*meshpp++, clip);
			phd_PopMatrix_I();

			phd_PushMatrix_I();
			phd_TranslateRel_I(bone[9], bone[10], bone[11]);
			gar_RotYXZsuperpack_I(&rot1, &rot2, 0);
			phd_RotZ_I(sub->FanRot);
			phd_PutPolygons_I(*meshpp++, clip);
			phd_PopMatrix_I();
		}
		else
		{
			phd_TranslateRel(frm[0][6], frm[0][7], frm[0][8]);
			rot1 = frm[0] + 9;

			gar_RotYXZsuperpack(&rot1, 0);
			phd_PutPolygons(*meshpp++, clip);

			phd_PushMatrix();
			phd_TranslateRel(bone[1], bone[2], bone[3]);
			gar_RotYXZsuperpack(&rot1, 0);
			phd_RotX(short(item->pos.z_rot + (sub->RotX >> 13)));
			phd_PutPolygons(*meshpp++, clip);
			phd_PopMatrix();

			phd_PushMatrix();
			phd_TranslateRel(bone[5], bone[6], bone[7]);
			gar_RotYXZsuperpack(&rot1, 0);
			phd_RotX(short((sub->RotX >> 13) - item->pos.z_rot));
			phd_PutPolygons(*meshpp++, clip);
			phd_PopMatrix();

			phd_PushMatrix();
			phd_TranslateRel(bone[9], bone[10], bone[11]);
			gar_RotYXZsuperpack(&rot1, 0);
			phd_RotZ(sub->FanRot);
			phd_PutPolygons(*meshpp++, clip);
			phd_PopMatrix();
		}
	}

	phd_PopMatrix();

	if (TriggerActive(item))
		S_DrawSubWakeFX(item);
}

static long CanGetOff(ITEM_INFO* item)
{
	SUBINFO* sub;
	FLOOR_INFO* floor;
	long rad, x, y, z, h, c;
	short room_number;

	sub = (SUBINFO*)item->data;

	if (lara.current_xvel || lara.current_zvel || sub->Vel)
		return 0;

	rad = WALL_SIZE * phd_cos(item->pos.x_rot) >> W2V_SHIFT;
	x = item->pos.x_pos + ((rad * phd_sin(item->pos.y_rot + 0x8000)) >> W2V_SHIFT);
	y = item->pos.y_pos - ((WALL_SIZE * phd_sin(item->pos.x_rot)) >> W2V_SHIFT);
	z = item->pos.z_pos + ((rad * phd_cos(item->pos.y_rot + 0x8000)) >> W2V_SHIFT);

	room_number = item->room_number;
	floor = GetFloor(x, y, z, &room_number);
	h = GetHeight(floor, x, y, z);

	if (h == NO_HEIGHT || y > h)
		return 0;

	c = GetCeiling(floor, x, y, z);

	if (h - c < 256 || y < c || c == NO_HEIGHT)
		return 0;

	return 1;
}

static void UserInput(ITEM_INFO* item, ITEM_INFO* l, SUBINFO* sub)
{
	PHD_VECTOR pos;
	GAME_VECTOR s;
	GAME_VECTOR t;
	long wh, wsd;
	short anim, frame;

	CanGetOff(item);
	anim = l->anim_number - objects[VEHICLE_ANIM].anim_index;
	frame = l->frame_number - anims[l->anim_number].frame_base;

	switch (l->current_anim_state)
	{
	case 0:

		if (anim == 0 && (frame == 16 || frame == 17))
		{
			pos.x = 0;
			pos.y = 0;
			pos.z = 0;
			GetLaraMeshPos(&pos, LMX_HIPS);

			l->pos.x_pos = pos.x;
			l->pos.y_pos = pos.y;
			l->pos.z_pos = pos.z;
			l->anim_number = ANIM_UWDEATH;
			l->frame_number = anims[ANIM_UWDEATH].frame_base;
			l->current_anim_state = AS_UWDEATH;
			l->goal_anim_state = AS_UWDEATH;
			l->fallspeed = 0;
			l->gravity_status = 0;
			l->pos.x_rot = 0;
			l->pos.z_rot = 0;
			sub->Flags |= 8;
		}

		item->speed = 0;
		break;

	case 2:

		if (anim == 9 && frame == 51)
		{
			pos.x = 0;
			pos.y = 0;
			pos.z = 0;
			GetWaterDepth(l->pos.x_pos, l->pos.y_pos, l->pos.z_pos, l->room_number);
			wh = GetWaterHeight(l->pos.x_pos, l->pos.y_pos, l->pos.z_pos, l->room_number);

			if (wh == NO_HEIGHT)
				wsd = NO_HEIGHT;
			else
				wsd = l->pos.y_pos - wh;

			GetLaraMeshPos(&pos, LMX_HIPS);
			l->pos.x_pos = pos.x;
			l->pos.y_pos = pos.y;
			l->pos.z_pos = pos.z;
			l->anim_number = ANIM_SURFTREAD;
			l->frame_number = anims[ANIM_SURFTREAD].frame_base;
			l->current_anim_state = AS_SURFTREAD;
			l->goal_anim_state = AS_SURFTREAD;
			l->fallspeed = 0;
			l->gravity_status = 0;
			l->pos.x_rot = 0;
			l->pos.z_rot = 0;
			UpdateLaraRoom(l, -381);
			lara.water_status = LARA_SURFACE;
			lara.water_surface_dist = -wsd;
			lara.dive_count = 11;
			lara.torso_x_rot = 0;
			lara.torso_y_rot = 0;
			lara.head_x_rot = 0;
			lara.head_y_rot = 0;
			lara.gun_status = 0;
			lara.skidoo = NO_ITEM;
			item->hit_points = 0;
		}

		break;

	case 4:

		if (l->hit_points <= 0)
		{
			l->goal_anim_state = 0;
			break;
		}

		if (input & IN_LEFT)
			sub->Rot -= 0x400000;
		else if (input & IN_RIGHT)
			sub->Rot += 0x400000;

		if (sub->Flags & 2)
		{
			if (item->pos.x_rot > 9100)
				item->pos.x_rot -= 182;
			else if (item->pos.x_rot < 9100)
				item->pos.x_rot += 182;
		}
		else if (input & IN_FORWARD)
			sub->RotX -= 0x16C0000;
		else if (input & IN_BACK)
			sub->RotX += 0x16C0000;

		if (input & IN_JUMP)
		{
			if (sub->Flags & 2 && input & IN_FORWARD && item->pos.x_rot > -2730)
				sub->Flags |= 4;

			sub->Vel += 0x40000;
		}
		else
			l->goal_anim_state = 5;

		break;

	case 5:

		if (l->hit_points <= 0)
		{
			l->goal_anim_state = 0;
			break;
		}

		if (input & IN_LEFT)
			sub->Rot -= 0x200000;
		else if (input & IN_RIGHT)
			sub->Rot += 0x200000;

		if (sub->Flags & 2)
		{
			if (item->pos.x_rot > 9100)
				item->pos.x_rot -= 182;
			else if (item->pos.x_rot < 9100)
				item->pos.x_rot += 182;
		}
		else if (input & IN_FORWARD)
			sub->RotX -= 0x16C0000;
		else if (input & IN_BACK)
			sub->RotX += 0x16C0000;

		if (input & IN_ROLL && CanGetOff(item))
		{
			if (sub->Flags & 2)
				l->goal_anim_state = 2;
			else
				l->goal_anim_state = 9;

			sub->Flags &= ~1;
			StopSoundEffect(SFX_LITTLE_SUB_LOOP);
			SoundEffect(SFX_LITTLE_SUB_STOP, &item->pos, SFX_ALWAYS);
		}
		else if (input & IN_JUMP)
		{
			if (sub->Flags & 2 && input & IN_FORWARD && item->pos.x_rot > -2730)
				sub->Flags |= 4;

			l->goal_anim_state = 4;
		}

		break;

	case 8:

		if (anim == 11)
		{
			item->pos.x_rot += 182;
			item->pos.y_pos += 4;

			if (frame == 30)
				SoundEffect(SFX_LITTLE_SUB_START, &item->pos, 2);

			if (frame == 50)
				sub->Flags |= 1;
		}
		else if (anim == 13)
		{
			if (frame == 30)
				SoundEffect(SFX_LITTLE_SUB_START, &item->pos, 2);

			if (frame == 42)
				sub->Flags |= 1;
		}

		break;

	case 9:

		if (anim == 12 && frame == 42)
		{
			//wc - the code here was glitchy so I simplified it to a simple pos update that is not using mgLOS()
			pos.x = 0;
			pos.y = 0;
			pos.z = 0;
			GetLaraMeshPos(&pos, LMX_HIPS);

			//wc - untrigger upv
			item->status = ITEM_INACTIVE;
			item->flags &= ~(IFL_CODEBITS);
			RemoveActiveItem(lara.skidoo);

			l->pos.x_pos = pos.x;
			l->pos.y_pos = pos.y;
			l->pos.z_pos = pos.z;
			l->anim_number = ANIM_TREAD;
			l->frame_number = anims[ANIM_TREAD].frame_base;
			l->current_anim_state = AS_TREAD;
			l->fallspeed = 0;
			l->gravity_status = 0;
			l->pos.x_rot = item->pos.x_rot;
			l->pos.z_rot = item->pos.z_rot;
			l->pos.y_rot = item->pos.y_rot;
			UpdateLaraRoom(l, pos.y);
			lara.water_status = LARA_UNDERWATER;
			lara.gun_status = LG_ARMLESS;
			lara.skidoo = NO_ITEM;
			item->hit_points = 0;
		}

		break;
	}

	if (sub->Flags & 4)
	{
		if (item->pos.x_rot > -2730)
			item->pos.x_rot -= 910;
		else
			sub->Flags &= ~4;
	}

	if (sub->Vel > 0)
	{
		sub->Vel -= 0x18000;

		if (sub->Vel < 0)
			sub->Vel = 0;

	}
	else if (sub->Vel < 0)
	{
		sub->Vel += 0x18000;

		if (sub->Vel > 0)
			sub->Vel = 0;
	}
	//wc - making the sub faster
	if (sub->Vel > 0x600000)
		sub->Vel = 0x600000;
	else if (sub->Vel < -0x400000)
		sub->Vel = -0x400000;

	if (sub->Rot > 0)
	{
		sub->Rot -= 0x100000;

		if (sub->Rot < 0)
			sub->Rot = 0;
	}
	else if (sub->Rot < 0)
	{
		sub->Rot += 0x100000;

		if (sub->Rot > 0)
			sub->Rot = 0;
	}

	if (sub->RotX > 0)
	{
		sub->RotX -= 0xB60000;

		if (sub->RotX < 0)
			sub->RotX = 0;
	}
	else if (sub->RotX < 0)
	{
		sub->RotX += 0xB60000;

		if (sub->RotX > 0)
			sub->RotX = 0;
	}

	if (sub->Rot > 0x1C00000)
		sub->Rot = 0x1C00000;
	else if (sub->Rot < -0x1C00000)
		sub->Rot = -0x1C00000;

	if (sub->RotX > 0x16C0000)
		sub->RotX = 0x16C0000;
	else if (sub->RotX < -0x16C0000)
		sub->RotX = -0x16C0000;
}

static void DoCurrent(ITEM_INFO* item)
{
	long sinkval, angle, speed, xvel, zvel, shifter, absvel;

	if (lara.current_active)
	{
		sinkval = lara.current_active - 1;
		angle = mGetAngle(camera.fixed[sinkval].x, camera.fixed[sinkval].z, lara_item->pos.x_pos, lara_item->pos.z_pos);
		angle = ((angle - 0x4000) >> 4) & 0xFFF;
		speed = camera.fixed[sinkval].data;
		xvel = (speed * rcossin_tbl[angle << 1]) >> 2;
		zvel = (speed * rcossin_tbl[(angle << 1) + 1]) >> 2;
		lara.current_xvel += short((xvel - lara.current_xvel) >> 4);
		lara.current_zvel += short((zvel - lara.current_zvel) >> 4);
	}
	else
	{
		absvel = abs(lara.current_xvel);

		if (absvel > 16)
			shifter = 4;
		else if (absvel > 8)
			shifter = 3;
		else
			shifter = 2;

		lara.current_xvel -= lara.current_xvel >> shifter;

		if (abs(lara.current_xvel) < 4)
			lara.current_xvel = 0;

		absvel = abs(lara.current_zvel);

		if (absvel > 16)
			shifter = 4;
		else if (absvel > 8)
			shifter = 3;
		else
			shifter = 2;

		lara.current_zvel -= lara.current_zvel >> shifter;

		if (abs(lara.current_zvel) < 4)
			lara.current_zvel = 0;

		if (!lara.current_xvel && !lara.current_zvel)
			return;
	}

	item->pos.x_pos += lara.current_xvel >> 8;
	item->pos.z_pos += lara.current_zvel >> 8;
	lara.current_active = 0;
}

static void FireSubHarpoon(ITEM_INFO* item)
{
	ITEM_INFO* bolt;
	PHD_VECTOR pos;
	short item_number;
	static char lr = 0;

	if (lara.harpoon.ammo <= 0)
		return;

	item_number = CreateItem();

	if (item_number == NO_ITEM)
		return;

	bolt = &items[item_number];
	bolt->object_number = HARPOON_BOLT;
	bolt->shade = -0x3DF0;
	bolt->room_number = item->room_number;
	pos.x = lr != 0 ? 22 : -22;
	pos.y = 24;
	pos.z = 230;
	GetJointAbsPosition(item, &pos, 3);
	bolt->pos.x_pos = pos.x;
	bolt->pos.y_pos = pos.y;
	bolt->pos.z_pos = pos.z;
	InitialiseItem(item_number);
	bolt->pos.x_rot = item->pos.x_rot;
	bolt->pos.y_rot = item->pos.y_rot;
	bolt->pos.z_rot = 0;
	bolt->fallspeed = short((-256 * phd_sin(bolt->pos.x_rot)) >> W2V_SHIFT);
	bolt->speed = short((256 * phd_cos(bolt->pos.x_rot)) >> W2V_SHIFT);
	bolt->hit_points = 256;
	bolt->item_flags[0] = 1;
	AddActiveItem(item_number);
	SoundEffect(SFX_LARA_HARPOON_FIRE_WATER, &lara_item->pos, SFX_ALWAYS);

	if (!savegame.bonus_flag)
		lara.harpoon.ammo--;

	savegame.ammo_used++;
	lr ^= 1;
}

static void BackgroundCollision(ITEM_INFO* item, ITEM_INFO* l, SUBINFO* sub)
{
	COLL_INFO coll;
	long h;

	coll.bad_pos = -NO_HEIGHT;
	coll.bad_neg = -400;
	coll.bad_ceiling = 400;
	coll.old.x = item->pos.x_pos;
	coll.old.y = item->pos.y_pos;
	coll.old.z = item->pos.z_pos;
	coll.radius = 300;
	coll.trigger = 0;
	coll.slopes_are_walls = 0;
	coll.slopes_are_pits = 0;
	coll.lava_is_pit = 0;
	coll.enable_spaz = 0;
	coll.enable_baddie_push = 1;

	if (item->pos.x_rot < -0x4000 || item->pos.x_rot > 0x4000)
		lara.move_angle = item->pos.y_rot + 0x8000;
	else
		lara.move_angle = item->pos.y_rot;

	coll.facing = lara.move_angle;
	h = (WALL_SIZE * phd_sin(item->pos.x_rot)) >> W2V_SHIFT;

	if (h < 0)
		h = -h;

	if (h < 200)
		h = 200;

	coll.bad_neg = -h;
	GetCollisionInfo(&coll, item->pos.x_pos, item->pos.y_pos + h / 2, item->pos.z_pos, item->room_number, h);
	ShiftItem(item, &coll);

	switch (coll.coll_type)
	{
	case CT_FRONT:

		if (sub->RotX > 0x1FFE0000)
			sub->RotX += 0x16C0000;
		else if (sub->RotX < -0x1FFE0000)
			sub->RotX -= 0x16C0000;
		else
			sub->Vel = 0;

		break;

	case CT_TOP:

		if (sub->RotX >= -0x1FFE0000)
			sub->RotX -= 0x16C0000;

		break;

	case CT_TOP_FRONT:
		sub->Vel = 0;
		break;

	case CT_LEFT:
		item->pos.y_rot += 910;
		break;

	case CT_RIGHT:
		item->pos.y_rot -= 910;
		break;

	case CT_CLAMP:
		item->pos.x_pos = coll.old.x;
		item->pos.y_pos = coll.old.y;
		item->pos.z_pos = coll.old.z;
		sub->Vel = 0;
		return;
	}

	if (coll.mid_floor < 0)
	{
		item->pos.y_pos += coll.mid_floor;
		sub->RotX += 0x16C0000;
	}
}

long SubControl()
{
	ITEM_INFO* item;
	SUBINFO* sub;
	FLOOR_INFO* floor;
	long wh;
	short room_number;

	item = &items[lara.skidoo];
	sub = (SUBINFO*)item->data;

	//wc - check for collision against smashable walls
	SubBaddieCollision(item);

	if (!(sub->Flags & 8))
	{
		UserInput(item, lara_item, sub);
		item->speed = sub->Vel >> 16;
		item->pos.x_rot += sub->RotX >> 16;
		item->pos.y_rot += sub->Rot >> 16;
		item->pos.z_rot = short(sub->Rot >> 12);

		if (item->pos.x_rot > 14560)
			item->pos.x_rot = 14560;
		else if (item->pos.x_rot < -14560)
			item->pos.x_rot = -14560;

		item->pos.x_pos += (phd_cos(item->pos.x_rot) * ((item->speed * phd_sin(item->pos.y_rot)) >> W2V_SHIFT)) >> W2V_SHIFT;
		item->pos.y_pos -= (item->speed * phd_sin(item->pos.x_rot)) >> W2V_SHIFT;
		item->pos.z_pos += (phd_cos(item->pos.x_rot) * ((item->speed * phd_cos(item->pos.y_rot)) >> W2V_SHIFT)) >> W2V_SHIFT;
	}

	room_number = item->room_number;
	floor = GetFloor(item->pos.x_pos, item->pos.y_pos, item->pos.z_pos, &room_number);
	long height = GetHeight(floor, item->pos.x_pos, item->pos.y_pos, item->pos.z_pos);
	long ceiling = GetCeiling(floor, item->pos.x_pos, item->pos.y_pos, item->pos.z_pos);

	bool validPosition = room[room_number].flags & ROOM_UNDERWATER;

	//wc - prevent sub from leaving the water
	if (!validPosition && item->speed > 0 && lara_item->current_anim_state != 2 && lara_item->current_anim_state != 9 && lara_item->current_anim_state != 8)
	{
		item->pos.x_pos -= (phd_cos(item->pos.x_rot) * ((item->speed * phd_sin(item->pos.y_rot)) >> W2V_SHIFT)) >> W2V_SHIFT;
		item->pos.y_pos += (item->speed * phd_sin(item->pos.x_rot)) >> W2V_SHIFT;
		item->pos.z_pos -= (phd_cos(item->pos.x_rot) * ((item->speed * phd_cos(item->pos.y_rot)) >> W2V_SHIFT)) >> W2V_SHIFT;
		sub->Flags &= ~4;
		item->speed = 0;
		sub->Vel = 0;
		return 0;
	}
	item->floor = height;

	if (sub->Flags & 1 && !(sub->Flags & 8))
	{
		wh = GetWaterHeight(item->pos.x_pos, item->pos.y_pos, item->pos.z_pos, room_number);

		if (wh != NO_HEIGHT && !(room[item->room_number].flags & ROOM_UNDERWATER))
		{
			if (wh - item->pos.y_pos >= -210)
				item->pos.y_pos = wh + 210;

			if (!(sub->Flags & 2))
			{
				SoundEffect(SFX_LARA_BREATH, &lara_item->pos, SFX_ALWAYS);
				sub->Flags &= ~4;
			}

			sub->Flags |= 2;
		}
		else if (wh != NO_HEIGHT && wh - item->pos.y_pos >= -210)
		{
			item->pos.y_pos = wh + 210;

			if (!(sub->Flags & 2))
			{
				SoundEffect(SFX_LARA_BREATH, &lara_item->pos, SFX_ALWAYS);
				sub->Flags &= ~4;
			}

			sub->Flags |= 2;
		}
		else
			sub->Flags &= ~2;

		if (sub->Flags & 2)
		{
			if (lara_item->hit_points >= 0)
			{
				lara.air += 10;

				if (lara.air > 1800)
					lara.air = 1800;
			}
		}
		else if (lara_item->hit_points >= 0)
		{
			if (!((lara_item->ai_bits & 16) && (lara_item->ai_bits & 1) == 0 && lara.air < 1800)) // wc - aqua lung upgrade does not deplete air
				lara.air--;

			if (lara.air < 0)
			{
				lara.air = -1;
				lara_item->hit_points -= 5;
			}
		}
	}

	TestTriggers(trigger_index, 0);

	if (lara.skidoo == NO_ITEM)
	{
		if (!(sub->Flags & 8))
			return 0;
	}
	else if (!(sub->Flags & 8))
	{
		DoCurrent(item);

		if (input & IN_ACTION && sub->Flags & 1 && !sub->WeaponTimer)
		{
			FireSubHarpoon(item);
			sub->WeaponTimer = 15;
		}

		if (room_number != item->room_number)
		{
			ItemNewRoom(lara.skidoo, room_number);
			ItemNewRoom(lara.item_number, room_number);
		}

		lara_item->pos.x_pos = item->pos.x_pos;
		lara_item->pos.y_pos = item->pos.y_pos + 128;
		lara_item->pos.z_pos = item->pos.z_pos;
		lara_item->pos.x_rot = item->pos.x_rot;
		lara_item->pos.y_rot = item->pos.y_rot;
		lara_item->pos.z_rot = item->pos.z_rot;
		AnimateItem(lara_item);
		BackgroundCollision(item, lara_item, sub);

		if (sub->Flags & 1)
			SoundEffect(SFX_LITTLE_SUB_LOOP, &item->pos, (item->speed << 16) | 0x1000000 | SFX_SETPITCH | SFX_ALWAYS);

		item->anim_number = lara_item->anim_number + objects[UPV].anim_index - objects[VEHICLE_ANIM].anim_index;
		item->frame_number = lara_item->frame_number + anims[item->anim_number].frame_base - anims[lara_item->anim_number].frame_base;
		camera.target_elevation = sub->Flags & 2 ? -10920 : 0;
		return 1;
	}

	AnimateItem(lara_item);

	if (room_number != item->room_number)
		ItemNewRoom(lara.skidoo, room_number);

	BackgroundCollision(item, lara_item, sub);
	sub->RotX = 0;
	item->anim_number = 5;
	item->frame_number = anims[objects[UPV].anim_index + 5].frame_base;
	item->current_anim_state = 5;
	item->goal_anim_state = 5;
	item->fallspeed = 0;
	item->gravity_status = 1;
	item->speed = 0;
	AnimateItem(item);
	return 1;
}

static void TriggerSubMist(long x, long y, long z, long speed, short angle)
{
	SPARKS* sptr;

	sptr = &sparks[GetFreeSpark()];
	sptr->On = 1;
	sptr->sR = 0;
	sptr->sG = 0;
	sptr->sB = 0;
	sptr->dR = 64;
	sptr->dG = 64;
	sptr->dB = 64;
	sptr->FadeToBlack = 12;
	sptr->ColFadeSpeed = (GetRandomControl() & 3) + 4;
	sptr->TransType = 2;
	sptr->extras = 0;
	sptr->Life = (GetRandomControl() & 3) + 20;
	sptr->sLife = sptr->Life;
	sptr->Dynamic = -1;
	sptr->x = (GetRandomControl() & 0xF) + x - 8;
	sptr->y = (GetRandomControl() & 0xF) + y - 8;
	sptr->z = (GetRandomControl() & 0xF) + z - 8;
	sptr->Xvel = (GetRandomControl() & 0x7F) + ((speed * phd_sin(angle)) >> (W2V_SHIFT + 2)) - 64;
	sptr->Yvel = 0;
	sptr->Zvel = (GetRandomControl() & 0x7F) + ((speed * phd_cos(angle)) >> (W2V_SHIFT + 2)) - 64;
	sptr->Friction = 3;

	if (GetRandomControl() & 1)
	{
		sptr->Flags = SF_ALTDEF | SF_ROTATE | SF_DEF | SF_SCALE;
		sptr->RotAng = GetRandomControl() & 0xFFF;

		if (GetRandomControl() & 1)
			sptr->RotAdd = -16 - (GetRandomControl() & 0xF);
		else
			sptr->RotAdd = (GetRandomControl() & 0xF) + 16;
	}
	else
		sptr->Flags = SF_ALTDEF | SF_DEF | SF_SCALE;

	sptr->Scalar = 3;
	sptr->Def = (uchar)objects[EXPLOSION1].mesh_index;
	sptr->MaxYvel = 0;
	sptr->Gravity = 0;
	sptr->dWidth = uchar((GetRandomControl() & 7) + (speed >> 1) + 16);
	sptr->sWidth = sptr->dWidth >> 2;
	sptr->Width = sptr->sWidth;
	sptr->sHeight = sptr->sWidth;
	sptr->Height = sptr->sWidth;
	sptr->dHeight = sptr->dWidth;
}

static void DoWake(ITEM_INFO* item, short lr)
{
	BITE_INFO* bite;
	PHD_VECTOR pos;

	if (TriggerActive(item) && !SubWakePts[SubCurrentStartWake][lr].life)
	{
		SubWakePts[SubCurrentStartWake][lr].life = 64;

		for (int i = 0; i < 2; i++)
		{
			bite = &sub_bites[(lr << 1) + 2 + i];
			pos.x = bite->x;
			pos.y = bite->y;
			pos.z = bite->z;
			GetJointAbsPosition(item, &pos, bite->mesh_num);
			SubWakePts[SubCurrentStartWake][lr].x[i] = pos.x;
			SubWakePts[SubCurrentStartWake][lr].y[i] = pos.y + 128;
			SubWakePts[SubCurrentStartWake][lr].z[i] = pos.z;
		}

		if (lr == 1)
			SubCurrentStartWake = (SubCurrentStartWake + 1) & 0x1F;
	}
}

static void UpdateWakeFX()
{
	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < 32; j++)
		{
			if (SubWakePts[j][i].life)
				SubWakePts[j][i].life--;
		}
	}
}

void SubEffects(short item_number)
{
	ITEM_INFO* item;
	SUBINFO* sub;
	PHD_VECTOR pos;
	PHD_3DPOS bPos;
	GAME_VECTOR s, t;
	long c;
	short room_number;

	item = &items[item_number];
	sub = (SUBINFO*)item->data;

	if (lara.skidoo == item_number)
	{
		if (sub->Vel)
		{
			sub->FanRot += short(sub->Vel >> 12);
			pos.x = sub_bites[0].x;
			pos.y = sub_bites[0].y;
			pos.z = sub_bites[0].z;
			GetJointAbsPosition(item, &pos, sub_bites[0].mesh_num);
			TriggerSubMist(pos.x, pos.y + 128, pos.z, abs(sub->Vel) >> 16, item->pos.y_rot + 0x8000);

			if (!(GetRandomControl() & 1))
			{
				bPos.x_pos = (GetRandomControl() & 0x3F) + pos.x - 32;
				bPos.y_pos = pos.y + 128;
				bPos.z_pos = (GetRandomControl() & 0x3F) + pos.z - 32;
				room_number = item->room_number;
				GetFloor(bPos.x_pos, bPos.y_pos, bPos.z_pos, &room_number);
				CreateBubble(&bPos, room_number, 4, 8);
			}
		}
		else
			sub->FanRot += 364;
	}

	for (int i = 0; i < 2; i++)
	{
		pos.x = sub_bites[1].x;
		pos.y = sub_bites[1].y;
		pos.z = sub_bites[1].z << (6 * i);
		GetJointAbsPosition(item, &pos, sub_bites[1].mesh_num);
		c = 255 - (GetRandomControl() & 0x1F);

		if (i == 1)
		{
			t.x = pos.x;
			t.y = pos.y;
			t.z = pos.z;
			t.room_number = item->room_number;
			LOS(&s, &t);
			pos.x = t.x;
			pos.y = t.y;
			pos.z = t.z;
		}
		else
		{
			s.x = pos.x;
			s.y = pos.y;
			s.z = pos.z;
			s.room_number = item->room_number;
		}

		TriggerDynamic(pos.x, pos.y, pos.z, 8 * i + 16, c, c, c);
	}

	if (!(wibble & 0xF) && sub->Vel)
	{
		DoWake(item, 0);
		DoWake(item, 1);
	}

	if (!sub->Vel)
	{
		if (SubWakeShade)
			SubWakeShade--;
	}
	else if (SubWakeShade < 16)
		SubWakeShade += 2;

	if (sub->WeaponTimer)
		sub->WeaponTimer--;

	UpdateWakeFX();
}
