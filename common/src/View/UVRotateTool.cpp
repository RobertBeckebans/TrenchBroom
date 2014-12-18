/*
 Copyright (C) 2010-2014 Kristian Duske
 
 This file is part of TrenchBroom.
 
 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "UVRotateTool.h"

#include "PreferenceManager.h"
#include "Preferences.h"
#include "Model/BrushFace.h"
#include "Model/BrushEdge.h"
#include "Model/BrushVertex.h"
#include "Model/ChangeBrushFaceAttributesRequest.h"
#include "Renderer/Circle.h"
#include "Renderer/Renderable.h"
#include "Renderer/RenderBatch.h"
#include "Renderer/Shaders.h"
#include "Renderer/RenderContext.h"
#include "Renderer/ShaderManager.h"
#include "Renderer/Transformation.h"
#include "Renderer/Vbo.h"
#include "Renderer/VertexArray.h"
#include "View/MapDocument.h"
#include "View/InputState.h"
#include "View/UVViewHelper.h"

namespace TrenchBroom {
    namespace View {
        const Hit::HitType UVRotateTool::AngleHandleHit = Hit::freeHitType();
        const float UVRotateTool::CenterHandleRadius =  2.5f;
        const float UVRotateTool::RotateHandleRadius = 32.0f;
        const float UVRotateTool::RotateHandleWidth  =  5.0f;

        UVRotateTool::UVRotateTool(MapDocumentWPtr document, UVViewHelper& helper) :
        ToolImpl(document),
        m_helper(helper) {}
        
        void UVRotateTool::doPick(const InputState& inputState, Hits& hits) {
            if (!m_helper.valid())
                return;

            const Model::BrushFace* face = m_helper.face();
            const Mat4x4 fromFace = face->fromTexCoordSystemMatrix(Vec2f::Null, Vec2f::One, true);

            const Plane3& boundary = face->boundary();
            const Mat4x4 toPlane = planeProjectionMatrix(boundary.distance, boundary.normal);

            const Ray3& pickRay = inputState.pickRay();
            const FloatType distance = pickRay.intersectWithPlane(boundary.normal, boundary.anchor());
            assert(!Math::isnan(distance));
            const Vec3 hitPoint = pickRay.pointAtDistance(distance);
            
            const Vec3 originOnPlane   = toPlane * fromFace * Vec3(m_helper.originInFaceCoords());
            const Vec3 hitPointOnPlane = toPlane * hitPoint;

            const float zoom = m_helper.cameraZoom();
            const FloatType error = std::abs(RotateHandleRadius / zoom - hitPointOnPlane.distanceTo(originOnPlane));
            if (error <= RotateHandleWidth / zoom)
                hits.addHit(Hit(AngleHandleHit, distance, hitPoint, 0, error));
        }
        
        bool UVRotateTool::doStartMouseDrag(const InputState& inputState) {
            assert(m_helper.valid());
            
            if (!inputState.modifierKeysPressed(ModifierKeys::MKNone) ||
                !inputState.mouseButtonsPressed(MouseButtons::MBLeft))
                return false;

            const Hits& hits = inputState.hits();
            const Hit& angleHandleHit = hits.findFirst(AngleHandleHit, true);

            if (!angleHandleHit.isMatch())
                return false;

            const Model::BrushFace* face = m_helper.face();
            const Mat4x4 toFace = face->toTexCoordSystemMatrix(Vec2f::Null, Vec2f::One, true);

            const Vec2f hitPointInFaceCoords(toFace * angleHandleHit.hitPoint());
            m_initalAngle = measureAngle(hitPointInFaceCoords) - face->rotation();

            document()->beginTransaction("Rotate Texture");
            
            return true;
        }
        
        bool UVRotateTool::doMouseDrag(const InputState& inputState) {
            assert(m_helper.valid());
            
            Model::BrushFace* face = m_helper.face();
            const Plane3& boundary = face->boundary();
            const Ray3& pickRay = inputState.pickRay();
            const FloatType curPointDistance = pickRay.intersectWithPlane(boundary.normal, boundary.anchor());
            const Vec3 curPoint = pickRay.pointAtDistance(curPointDistance);
            
            const Mat4x4 toFaceOld = face->toTexCoordSystemMatrix(Vec2f::Null, Vec2f::One, true);
            const Mat4x4 toWorld = face->fromTexCoordSystemMatrix(Vec2f::Null, Vec2f::One, true);

            const Vec2f curPointInFaceCoords(toFaceOld * curPoint);
            const float curAngle = measureAngle(curPointInFaceCoords);

            const float angle = curAngle - m_initalAngle;
            const float snappedAngle = Math::correct(snapAngle(angle), 4, 0.0f);

            const Vec2f oldCenterInFaceCoords = m_helper.originInFaceCoords();
            const Vec3 oldCenterInWorldCoords = toWorld * Vec3(oldCenterInFaceCoords);
            
            Model::ChangeBrushFaceAttributesRequest request;
            request.setRotation(snappedAngle);
            document()->setFaceAttributes(request);
            
            // Correct the offsets and the position of the rotation center.
            const Mat4x4 toFaceNew = face->toTexCoordSystemMatrix(Vec2f::Null, Vec2f::One, true);
            const Vec2f newCenterInFaceCoords(toFaceNew * oldCenterInWorldCoords);
            m_helper.setOrigin(newCenterInFaceCoords);

            const Vec2f delta = (oldCenterInFaceCoords - newCenterInFaceCoords) / face->scale();
            const Vec2f newOffset = (face->offset() + delta).corrected(4, 0.0f);
            
            request.clear();
            request.setOffset(newOffset);
            document()->setFaceAttributes(request);
            
            return true;
        }
        
        float UVRotateTool::measureAngle(const Vec2f& point) const {
            const Model::BrushFace* face = m_helper.face();
            const Vec2f origin = m_helper.originInFaceCoords();
            return Math::mod(face->measureTextureAngle(origin, point), 360.0f);
        }
        
        float UVRotateTool::snapAngle(const float angle) const {
            const Model::BrushFace* face = m_helper.face();
            
            const float angles[] = {
                Math::mod(angle +   0.0f, 360.0f),
                Math::mod(angle +  90.0f, 360.0f),
                Math::mod(angle + 180.0f, 360.0f),
                Math::mod(angle + 270.0f, 360.0f),
            };
            float minDelta = std::numeric_limits<float>::max();
            
            const Mat4x4 toFace = face->toTexCoordSystemMatrix(Vec2f::Null, Vec2f::One, true);
            const Model::BrushEdgeList& edges = face->edges();
            Model::BrushEdgeList::const_iterator it, end;
            for (it = edges.begin(), end = edges.end(); it != end; ++it) {
                const Model::BrushEdge* edge = *it;
                
                const Vec3 startInFaceCoords = toFace * edge->start->position;
                const Vec3 endInFaceCoords   = toFace * edge->end->position;
                const float edgeAngle        = Math::mod(face->measureTextureAngle(startInFaceCoords, endInFaceCoords), 360.0f);
                
                for (size_t i = 0; i < 4; ++i) {
                    if (std::abs(angles[i] - edgeAngle) < std::abs(minDelta))
                        minDelta = angles[i] - edgeAngle;
                }
            }
            
            if (std::abs(minDelta) < 3.0f)
                return angle - minDelta;
            return angle;
        }

        void UVRotateTool::doEndMouseDrag(const InputState& inputState) {
            document()->endTransaction();
        }
        
        void UVRotateTool::doCancelMouseDrag() {
            document()->rollbackTransaction();
            document()->endTransaction();
        }

        class UVRotateTool::Render : public Renderer::Renderable {
        private:
            const UVViewHelper& m_helper;
            bool m_highlight;
            Renderer::Circle m_center;
            Renderer::Circle m_outer;
        public:
            Render(const UVViewHelper& helper, const float centerRadius, const float outerRadius, const bool highlight) :
            m_helper(helper),
            m_highlight(highlight),
            m_center(makeCircle(helper, centerRadius, 10, true)),
            m_outer(makeCircle(helper, outerRadius, 32, false)) {}
        private:
            static Renderer::Circle makeCircle(const UVViewHelper& helper, const float radius, const size_t segments, const bool fill) {
                const float zoom = helper.cameraZoom();
                return Renderer::Circle(radius / zoom, segments, fill);
            }
        private:
            void doPrepare(Renderer::Vbo& vbo) {
                m_center.prepare(vbo);
                m_outer.prepare(vbo);
            }
            
            void doRender(Renderer::RenderContext& renderContext) {
                const Model::BrushFace* face = m_helper.face();
                const Mat4x4 fromFace = face->fromTexCoordSystemMatrix(Vec2f::Null, Vec2f::One, true);
                
                const Plane3& boundary = face->boundary();
                const Mat4x4 toPlane = planeProjectionMatrix(boundary.distance, boundary.normal);
                const Mat4x4 fromPlane = invertedMatrix(toPlane);

                const Vec2f originPosition(toPlane * fromFace * Vec3(m_helper.originInFaceCoords()));
                const Vec2f faceCenterPosition(toPlane * m_helper.face()->boundsCenter());

                const Color& handleColor = pref(Preferences::HandleColor);
                const Color& highlightColor = pref(Preferences::SelectedHandleColor);

                Renderer::ActiveShader shader(renderContext.shaderManager(), Renderer::Shaders::VaryingPUniformCShader);
                const Renderer::MultiplyModelMatrix toWorldTransform(renderContext.transformation(), fromPlane);
                {
                    const Mat4x4 translation = translationMatrix(Vec3(originPosition));
                    const Renderer::MultiplyModelMatrix centerTransform(renderContext.transformation(), translation);
                    if (m_highlight)
                        shader.set("Color", highlightColor);
                    else
                        shader.set("Color", handleColor);
                    m_outer.render();
                }
                
                {
                    const Mat4x4 translation = translationMatrix(Vec3(faceCenterPosition));
                    const Renderer::MultiplyModelMatrix centerTransform(renderContext.transformation(), translation);
                    shader.set("Color", highlightColor);
                    m_center.render();
                }
            }
        };
        
        void UVRotateTool::doRender(const InputState& inputState, Renderer::RenderContext& renderContext, Renderer::RenderBatch& renderBatch) {
            if (!m_helper.valid())
                return;
            
            const Hits& hits = inputState.hits();
            const Hit& angleHandleHit = hits.findFirst(AngleHandleHit, true);
            const bool highlight = angleHandleHit.isMatch() || dragging();
            
            renderBatch.addOneShot(new Render(m_helper, CenterHandleRadius, RotateHandleRadius, highlight));
        }
    }
}
