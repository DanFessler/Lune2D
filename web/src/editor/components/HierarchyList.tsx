import { Dockable } from "@danfessler/react-dockable";
import { useState } from "react";
import type { HierarchyEntity } from "../../engineBridge";
import { PiBoundingBoxFill } from "react-icons/pi";
import { useDraggable, useDroppable } from "@dnd-kit/core";
import styles from "./HierarchyList.module.css";

export type HierarchyListProps = {
  entities: HierarchyEntity[];
  selectedId: string | null;
  onSelect: (id: string) => void;
  onDeleteEntity?: (id: string) => void | Promise<void>;
  isDragging?: boolean;
  canDrop?: boolean;
};

/** Multiple roots: one ctx-style subtree per entity (avoids nested <ul> per row). */
function HierarchyList({
  entities,
  selectedId,
  onSelect,
  onDeleteEntity,
  isDragging: isDraggingProp = false,
  canDrop = false,
}: HierarchyListProps) {
  return (
    <div className={styles.forest}>
      {entities.map((entity) => (
        <HierarchySubtree
          key={entity.id}
          entity={entity}
          selectedId={selectedId}
          onSelect={onSelect}
          onDeleteEntity={onDeleteEntity}
          isDragging={isDraggingProp}
          canDrop={canDrop}
        />
      ))}
    </div>
  );
}

/** Mirrors ctx-game `HierarchyList` for a single node: one `<ul>` row + bottom divider. */
function HierarchySubtree({
  entity,
  selectedId,
  onSelect,
  onDeleteEntity,
  isDragging: isDraggingProp,
  canDrop,
}: {
  entity: HierarchyEntity;
  selectedId: string | null;
  onSelect: (id: string) => void;
  onDeleteEntity?: (id: string) => void | Promise<void>;
  isDragging: boolean;
  canDrop: boolean;
}) {
  const { setNodeRef, attributes, listeners, isDragging } = useDraggable({
    id: entity.id,
    data: {
      children: (
        <ul className={styles.list}>
          <HierarchyItem
            entity={entity}
            selectedId={selectedId}
            onSelect={onSelect}
            onDeleteEntity={onDeleteEntity}
            isDragging={true}
            canDrop={canDrop}
          />
        </ul>
      ),
      type: "game-object",
      objectId: entity.id,
    },
  });

  const draggableProps = canDrop
    ? {
        ref: setNodeRef,
        ...attributes,
        ...listeners,
        style: { opacity: isDragging ? 0.25 : 1 },
      }
    : {};

  return (
    <ul className={styles.list} {...draggableProps}>
      <HierarchyItem
        entity={entity}
        selectedId={selectedId}
        onSelect={onSelect}
        onDeleteEntity={onDeleteEntity}
        isDragging={isDragging || isDraggingProp}
        canDrop={canDrop}
      />
      {!(isDragging || isDraggingProp) && canDrop && (
        <DroppableDivider side="bottom" id={entity.id} />
      )}
    </ul>
  );
}

function HierarchyItem({
  entity,
  selectedId,
  onSelect,
  onDeleteEntity,
  isDragging,
  canDrop,
}: {
  entity: HierarchyEntity;
  selectedId: string | null;
  onSelect: (id: string) => void;
  onDeleteEntity?: (id: string) => void | Promise<void>;
  isDragging: boolean;
  canDrop: boolean;
}) {
  const [isOpen, setIsOpen] = useState(true);
  const hasChildren = entity.children.length > 0;

  const { setNodeRef, isOver } = useDroppable({
    id: entity.id,
    data: {
      type: "reparent-object",
      objectId: entity.id,
    },
  });

  const row = (
    <div
      className={`${styles.item} ${
        selectedId === entity.id || isOver ? styles.selected : ""
      }`}
      onClick={() => onSelect(entity.id)}
    >
      {hasChildren ? (
        <div
          role="button"
          tabIndex={0}
          onKeyDown={(e) => {
            if (e.key === "Enter" || e.key === " ") {
              e.preventDefault();
              setIsOpen(!isOpen);
            }
          }}
          onClick={(e) => {
            e.stopPropagation();
            setIsOpen(!isOpen);
          }}
          className={styles.arrow}
        >
          {isOpen ? "▼" : "▶"}
        </div>
      ) : null}
      <PiBoundingBoxFill
        style={{ width: "14px", height: "14px", flexShrink: 0 }}
      />
      {entity.name}
      {!isDragging && canDrop && (
        <>
          <DroppableDivider side="top" id={entity.id} />
          <div
            ref={setNodeRef}
            style={{
              position: "absolute",
              left: 0,
              width: "calc(100% - 24px)",
              height: 4,
              top: "50%",
              marginLeft: 24,
            }}
          />
        </>
      )}
    </div>
  );

  return (
    <li>
      {onDeleteEntity ? (
        <Dockable.Menu
          id={`hierarchy-ctx-${entity.id}`}
          mode="context"
          customItems={[
            {
              items: [
                {
                  label: "Delete",
                  onClick: () => {
                    void onDeleteEntity(entity.id);
                  },
                },
              ],
            },
          ]}
        >
          {row}
        </Dockable.Menu>
      ) : (
        row
      )}
      {isOpen && hasChildren ? (
        <div className={styles.children}>
          {entity.children.map((child) => (
            <HierarchySubtree
              key={child.id}
              entity={child}
              selectedId={selectedId}
              onSelect={onSelect}
              onDeleteEntity={onDeleteEntity}
              isDragging={isDragging}
              canDrop={true}
            />
          ))}
        </div>
      ) : null}
    </li>
  );
}

function DroppableDivider({ side, id }: { side: "top" | "bottom"; id: string }) {
  const { setNodeRef, isOver } = useDroppable({
    id: `${id}-${side}`,
    data: {
      objectId: id,
      side,
    },
  });

  return (
    <div
      ref={setNodeRef}
      className={styles.divider}
      style={{
        backgroundColor: isOver
          ? "var(--dockable-colors-selected)"
          : "transparent",
        [side]: -2,
      }}
    />
  );
}

export default HierarchyList;
