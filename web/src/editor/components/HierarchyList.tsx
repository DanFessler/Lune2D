import { useState } from "react";
import type { HierarchyEntity } from "../../engineBridge";
import { PiBoundingBoxFill } from "react-icons/pi";
import { useDraggable, useDroppable } from "@dnd-kit/core";
import styles from "./HierarchyList.module.css";

export type HierarchyListProps = {
  entities: HierarchyEntity[];
  selectedId: string | null;
  onSelect: (id: string) => void;
  isDragging?: boolean;
  canDrop?: boolean;
};

function HierarchyList({
  entities,
  selectedId,
  onSelect,
  isDragging: isDraggingProp = false,
  canDrop = false,
}: HierarchyListProps) {
  return (
    <ul className={styles.list}>
      {entities.map((entity) => (
        <HierarchyListRow
          key={entity.id}
          entity={entity}
          selectedId={selectedId}
          onSelect={onSelect}
          isDragging={isDraggingProp}
          canDrop={canDrop}
        />
      ))}
    </ul>
  );
}

function HierarchyListRow({
  entity,
  selectedId,
  onSelect,
  isDragging: isDraggingProp,
  canDrop,
}: {
  entity: HierarchyEntity;
  selectedId: string | null;
  onSelect: (id: string) => void;
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
            isDragging={true}
            canDrop={canDrop}
          />
        </ul>
      ),
      type: "game-object",
      entityId: entity.id,
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
  isDragging,
  canDrop,
}: {
  entity: HierarchyEntity;
  selectedId: string | null;
  onSelect: (id: string) => void;
  isDragging: boolean;
  canDrop: boolean;
}) {
  const [isOpen, setIsOpen] = useState(true);
  const hasChildren = entity.children.length > 0;

  const { setNodeRef, isOver } = useDroppable({
    id: entity.id,
    data: {
      type: "reparent-object",
      entityId: entity.id,
    },
  });

  return (
    <li>
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
      {isOpen && hasChildren ? (
        <div className={styles.children}>
          {entity.children.map((child) => (
            <HierarchyListRow
              key={child.id}
              entity={child}
              selectedId={selectedId}
              onSelect={onSelect}
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
    id: id + "-" + side,
    data: {
      entityId: id,
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
