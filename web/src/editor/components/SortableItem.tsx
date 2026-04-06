/**
 * @dnd-kit sortable wrapper (ctx-game parity). Use when building draggable inspector rows.
 *
 * Drag activates only from the node that receives `setActivatorNodeRef` + `listeners`
 * (see `handle` render prop). Prevents conflicts with sliders, inputs, and portaled UI.
 */
import type { DraggableSyntheticListeners } from "@dnd-kit/core";
import { useSortable } from "@dnd-kit/sortable";
import { CSS } from "@dnd-kit/utilities";
import type { CSSProperties, ReactNode } from "react";

export type SortableDragHandleProps = {
  setActivatorNodeRef: (element: HTMLElement | null) => void;
  listeners: DraggableSyntheticListeners;
};

type Props = {
  id: string;
  /** Shown in DragOverlay; should match `children(handle)` but without needing handle props. */
  dragOverlay: ReactNode;
  children: (handle: SortableDragHandleProps) => ReactNode;
  style?: CSSProperties;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  data?: Record<string, any>;
  disabled?: boolean;
};

export function SortableItem({
  id,
  dragOverlay,
  children,
  style,
  data,
  disabled = false,
}: Props) {
  const {
    attributes,
    listeners,
    setNodeRef,
    setActivatorNodeRef,
    transform,
    isDragging,
    transition,
  } = useSortable({
    id,
    disabled,
    data: { ...data, children: dragOverlay },
  });

  const itemStyle: CSSProperties = {
    transform: CSS.Translate.toString(transform),
    transition,
    opacity: isDragging ? 0 : 1,
    ...style,
  };

  const handle: SortableDragHandleProps = {
    setActivatorNodeRef,
    listeners,
  };

  return (
    <div ref={setNodeRef} style={itemStyle} {...attributes}>
      {children(handle)}
    </div>
  );
}

export default SortableItem;
