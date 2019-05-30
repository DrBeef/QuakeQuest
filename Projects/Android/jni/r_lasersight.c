
#include "quakedef.h"
#include "image.h"

cvar_t r_lasersight_thickness = {CVAR_SAVE, "r_lasersight_thickness", "0.1", "thickness of the laser sight effect"};
cvar_t r_lasersight_color_red = {CVAR_SAVE, "r_lasersight_color_red", "0.8", "color of the laser sight effect"};
cvar_t r_lasersight_color_green = {CVAR_SAVE, "r_lasersight_color_green", "0.1", "color of the laser sight effect"};
cvar_t r_lasersight_color_blue = {CVAR_SAVE, "r_lasersight_color_blue", "0", "color of the laser sight effect"};

void R_LaserSights_Init(void)
{
	Cvar_RegisterVariable(&r_lasersight_thickness);
	Cvar_RegisterVariable(&r_lasersight_color_red);
	Cvar_RegisterVariable(&r_lasersight_color_green);
	Cvar_RegisterVariable(&r_lasersight_color_blue);
}

void R_DrawBLineMesh(vec3_t mins, vec3_t maxs, float thickness, float cr, float cg, float cb, float ca);
extern cvar_t r_lasersight;

void R_DrawLaserSights(void)
{
	vec3_t org, start, end, dir;
	vec_t dist;
	CL_LaserSight_CalculatePositions(start, end);

	// calculate the nearest point on the line (beam) for depth sorting
	VectorSubtract(end, start, dir);
	dist = (DotProduct(r_refdef.view.origin, dir) - DotProduct(start, dir)) / (DotProduct(end, dir) - DotProduct(start, dir));
	dist = bound(0, dist, 1);
	VectorLerp(start, dist, end, org);

	if (r_lasersight.integer == 1) {
	    //Beam
        GL_DepthTest(!r_showdisabledepthtest.integer);
        GL_CullFace(r_refdef.view.cullface_front);
        R_DrawBLineMesh(org, end, 0.3f, r_lasersight_color_red.value,
                        r_lasersight_color_green.value, r_lasersight_color_blue.value, 1);
    }
}

