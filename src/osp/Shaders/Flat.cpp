/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "Flat.h"

#include <osp/Active/SysRender.h>

#include <Magnum/Math/Color.h>

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using namespace osp;
using namespace osp::active;
using namespace osp::shader;

void shader::draw_ent_flat(
        ActiveEnt ent, ACompCamera const& camera,
        EntityToDraw::UserData_t userData) noexcept
{
    using Flag = Flat::Flag;

    auto &rData = *reinterpret_cast<ACtxDrawFlat*>(userData[0]);
    auto &rShader = *reinterpret_cast<Flat*>(userData[1]);

    // Collect uniform information
    ACompDrawTransform const &drawTf = rData.m_pDrawTf->get(ent);

    Magnum::Matrix4 entRelative = camera.m_inverse * drawTf.m_transformWorld;

    /* 4th component indicates light type. A value of 0.0f indicates that the
     * light is a direction light coming from the specified direction relative
     * to the camera.
     */
    //Vector4 light = ;

    if (rShader.flags() & Flag::Textured)
    {
        rShader.bindTexture(*rData.m_pDiffuseTexGl->get(ent).m_tex);
    }

    rShader
        .setColor(0xffffff00_rgbaf)
        .setTransformationProjectionMatrix(camera.m_projection*entRelative)
        .draw(*rData.m_pMeshGl->get(ent).m_mesh);
}


void shader::assign_flat(
        RenderGroup::ArrayView_t entities,
        RenderGroup::Storage_t *pStorageOpaque,
        RenderGroup::Storage_t *pStorageTransparent,
        acomp_view_t<ACompOpaque const> viewOpaque,
        acomp_view_t<ACompTextureGL const> viewDiffuse,
        ACtxDrawFlat &rData)
{

    for (ActiveEnt ent : entities)
    {
        if (viewOpaque.contains(ent))
        {
            if (pStorageOpaque == nullptr)
            {
                continue;
            }

            if (viewDiffuse.contains(ent))
            {
                pStorageOpaque->emplace(
                        ent, EntityToDraw{&draw_ent_flat, {&rData, &(*rData.m_shaderDiffuse)} });
            }
            else
            {
                pStorageOpaque->emplace(
                        ent, EntityToDraw{&draw_ent_flat, {&rData, &(*rData.m_shaderUntextured)} });
            }
        }
        else
        {

            if (pStorageTransparent == nullptr)
            {
                continue;
            }

            if (viewDiffuse.contains(ent))
            {
                pStorageTransparent->emplace(
                        ent, EntityToDraw{&draw_ent_flat, {&rData, &(*rData.m_shaderDiffuse)} });
            }
            else
            {
                pStorageTransparent->emplace(
                        ent, EntityToDraw{&draw_ent_flat, {&rData, &(*rData.m_shaderUntextured)} });
            }

        }


    }
}

