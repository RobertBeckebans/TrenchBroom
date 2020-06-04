/*
 Copyright (C) 2010-2017 Kristian Duske

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

#ifndef TrenchBroom_ObjectRenderer
#define TrenchBroom_ObjectRenderer

#include "Renderer/BrushRenderer.h"
#include "Renderer/EntityRenderer.h"
#include "Renderer/GroupRenderer.h"

#include <vector>

namespace TrenchBroom {
    class Color;
    class Logger;

    namespace Assets {
        class EntityModelManager;
    }

    namespace Model {
        class EditorContext;
        class EntityNode;
        class GroupNode;
    }

    namespace Renderer {
        class FontManager;
        class RenderBatch;

        class ObjectRenderer {
        private:
            GroupRenderer m_groupRenderer;
            EntityRenderer m_entityRenderer;
        public:
            ObjectRenderer(Logger& logger, Assets::EntityModelManager& entityModelManager, const Model::EditorContext& editorContext) :
            m_groupRenderer(editorContext),
            m_entityRenderer(logger, entityModelManager, editorContext) {}
        public: // object management
            void setObjects(const std::vector<Model::GroupNode*>& groups, const std::vector<Model::EntityNode*>& entities);
            void invalidate();
            void clear();
            void reloadModels();
        public: // configuration
            void setShowOverlays(bool showOverlays);
            void setEntityOverlayTextColor(const Color& overlayTextColor);
            void setGroupOverlayTextColor(const Color& overlayTextColor);
            void setOverlayBackgroundColor(const Color& overlayBackgroundColor);

            void setTint(bool tint);
            void setTintColor(const Color& tintColor);

            void setShowOccludedObjects(bool showOccludedObjects);
            void setOccludedEdgeColor(const Color& occludedEdgeColor);

            void setShowEntityAngles(bool showAngles);
            void setEntityAngleColor(const Color& color);

            void setOverrideGroupBoundsColor(bool overrideGroupBoundsColor);
            void setGroupBoundsColor(const Color& color);

            void setOverrideEntityBoundsColor(bool overrideEntityBoundsColor);
            void setEntityBoundsColor(const Color& color);

            void setShowHiddenObjects(bool showHiddenObjects);
        public: // rendering
            void renderOpaque(RenderContext& renderContext, RenderBatch& renderBatch);
        private:
            ObjectRenderer(const ObjectRenderer&);
            ObjectRenderer& operator=(const ObjectRenderer&);
        };
    }
}

#endif /* defined(TrenchBroom_ObjectRenderer) */
