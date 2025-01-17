#include "stdafx.h"
#pragma hdrstop

#include "Blender_Vertex.h"

#if RENDER != R_R1
#error "The blender can't be used in this renderer generation"
#endif

CBlender_Vertex::CBlender_Vertex()
{
    description.CLS = B_VERT;
    description.version = 1;
    oTessellation.Count = 4;
    oTessellation.IDselected = 0;
}

LPCSTR CBlender_Vertex::getComment()
{
    return "LEVEL: diffuse*base";
}

BOOL CBlender_Vertex::canBeDetailed()
{
    return TRUE;
}

void CBlender_Vertex::Save(IWriter& fs)
{
    IBlender::Save(fs);

    xrPWRITE_PROP(fs, "Tessellation", xrPID_TOKEN, oTessellation);

    xrP_TOKEN::Item I;
    I.ID = 0;
    xr_strcpy(I.str, "NO_TESS");
    fs.w(&I, sizeof(I));
    I.ID = 1;
    xr_strcpy(I.str, "TESS_PN");
    fs.w(&I, sizeof(I));
    I.ID = 2;
    xr_strcpy(I.str, "TESS_HM");
    fs.w(&I, sizeof(I));
    I.ID = 3;
    xr_strcpy(I.str, "TESS_PN+HM");
    fs.w(&I, sizeof(I));
}

void CBlender_Vertex::Load(IReader& fs, u16 version)
{
    IBlender::Load(fs, version);

    if (version > 0)
    {
        xrPREAD_PROP(fs, xrPID_TOKEN, oTessellation);
    }
}

void CBlender_Vertex::Compile(CBlender_Compile& C)
{
    IBlender::Compile(C);

    if (C.bFFP)
        CompileFFP(C);
    else
        CompileProgrammable(C);
}

void CBlender_Vertex::CompileFFP(CBlender_Compile& C) const
{
    if (!ps_r1_flags.is_any(R1FLAG_FFP_LIGHTMAPS | R1FLAG_DLIGHTS))
    {
        C.PassBegin();
        {
            C.PassSET_LightFog(true, true);

            // Stage0 - Base texture
            C.StageBegin();
            C.StageSET_Color(D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_DIFFUSE);
            C.StageSET_Alpha(D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_DIFFUSE);
            C.Stage_Texture(oT_Name);
            C.Stage_Matrix(oT_xform, 0);
            C.Stage_Constant("$null");
            C.StageEnd();
        }
        C.PassEnd();
    }
    else
    {
        switch (C.iElement)
        {
        case SE_R1_NORMAL_HQ:
        case SE_R1_NORMAL_LQ:
        {
            // Level view
            C.PassBegin();
            {
                C.PassSET_ZB(TRUE, TRUE);
                C.PassSET_Blend(FALSE, D3DBLEND_ONE, D3DBLEND_ZERO, FALSE, 0);
                C.PassSET_LightFog(FALSE, TRUE);

                // Stage0 - Base texture
                C.StageBegin();
                C.StageSET_Color(D3DTA_TEXTURE, D3DTOP_MODULATE2X, D3DTA_DIFFUSE);
                C.StageSET_Alpha(D3DTA_TEXTURE, D3DTOP_MODULATE2X, D3DTA_DIFFUSE);
                C.Stage_Texture(oT_Name);
                C.Stage_Matrix(oT_xform, 0);
                C.Stage_Constant("$null");
                C.StageEnd();
            }
            C.PassEnd();
            break;
        }

        case SE_R1_LMODELS:
        {
            // Lighting only
            C.PassBegin();
            {
                C.PassSET_ZB(TRUE, TRUE);
                C.PassSET_Blend_SET();
                C.PassSET_LightFog(FALSE, FALSE);

                // Stage0 - diffuse
                C.StageBegin();
                C.StageSET_Color(D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_DIFFUSE);
                C.StageSET_Alpha(D3DTA_TEXTURE, D3DTOP_SELECTARG2, D3DTA_DIFFUSE);
                C.Stage_Texture("$null");
                C.Stage_Matrix("$null", 0);
                C.Stage_Constant("$null");
                C.StageEnd();
            }
            C.PassEnd();
            break;
        }

        default:
            break;
        } // switch (C.iElement)
    }
}

void CBlender_Vertex::CompileProgrammable(CBlender_Compile& C) const
{
    switch (C.iElement)
    {
    case SE_R1_NORMAL_HQ:
        // Level view
        C.PassBegin();
        {
            cpcstr tsv_hq = C.bDetail_Diffuse ? "vert_dt" : "vert";
            cpcstr tsp_hq = C.bDetail_Diffuse ? "vert_dt" : "vert";

            C.PassSET_Shaders(tsv_hq, tsp_hq);

            C.PassSET_LightFog(false, true);

            C.SampledImage("s_base", "s_base", C.L_textures[0]);
            if (C.bDetail_Diffuse)
            {
                C.SampledImage("s_detail", "s_detail", C.detail_texture);
            }
        }
        C.PassEnd();
        break;

    case SE_R1_NORMAL_LQ:
        // Level view
        C.PassBegin();
        {
            C.PassSET_Shaders("vert", "vert");

            C.PassSET_LightFog(false, true);

            C.SampledImage("s_base", "s_base", C.L_textures[0]);
        }
        C.PassEnd();
        break;

    case SE_R1_LPOINT:
        C.PassBegin();
        {
            C.PassSET_Shaders("vert_point", "add_point");

            C.PassSET_ZB(true, false);
            C.PassSET_Blend(true, D3DBLEND_ONE, D3DBLEND_ONE, true, 0);

            C.SampledImage("s_base", "s_base", C.L_textures[0]);
            C.SampledImage("smp_rtlinear", "s_lmap", TEX_POINT_ATT);
            C.SampledImage("smp_rtlinear", "s_att", TEX_POINT_ATT);;
        }
        C.PassEnd();
        break;

    case SE_R1_LSPOT:
        C.PassBegin();
        {
            C.PassSET_Shaders("vert_spot", "add_spot");

            C.PassSET_ZB(true, false);
            C.PassSET_Blend(true, D3DBLEND_ONE, D3DBLEND_ONE, true, 0);

            C.SampledImage("s_base", "s_base", C.L_textures[0]);
            u32 stage = C.SampledImage("smp_rtlinear", "s_lmap", "internal" DELIMITER "internal_light_att");
            {
                C.i_Projective(stage, true);
            }
            C.SampledImage("smp_rtlinear", "s_att", TEX_SPOT_ATT);
        }
        C.PassEnd();
        break;

    case SE_R1_LMODELS:
        C.PassBegin();
        {
            C.PassSET_Shaders("vert_l", "vert_l");

            C.SampledImage("s_base", "s_base", C.L_textures[0]);
        }
        C.PassEnd();
        break;
    } // switch (C.iElement)
}
