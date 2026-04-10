import type { TabProps } from "@danfessler/react-dockable";
import type { EngineEntity } from "../engineBridge";
import { engine } from "../engineProxy";
import { saveSceneUndoState } from "./sceneUndo";

export type TabMenuActions = NonNullable<TabProps["actions"]>;

export function sceneHierarchyTabActions(opts: {
  onNewEntity: () => void | Promise<void>;
}): TabMenuActions {
  return [
    {
      items: [
        {
          label: "New entity",
          onClick: () => {
            void opts.onNewEntity();
          },
        },
      ],
    },
  ];
}

export function inspectorTabActions(opts: {
  entity: EngineEntity | null;
  behaviorNames: string[];
  onNewScript: () => void;
}): TabMenuActions | undefined {
  if (!opts.entity) return undefined;

  const entityId = Number(opts.entity.id);

  const behaviorItems =
    opts.behaviorNames.length === 0
      ? [
          {
            label: "No behaviors registered",
            onClick: () => {},
          },
        ]
      : opts.behaviorNames.map((name) => ({
          label: name,
          onClick: () => {
            void engine.runtime.addScript(entityId, name).then(() => {
              saveSceneUndoState();
            });
          },
        }));

  return [
    {
      items: [
        {
          label: "New script…",
          onClick: () => opts.onNewScript(),
        },
        {
          label: "Add Behavior",
          items: behaviorItems,
        },
      ],
    },
  ];
}
