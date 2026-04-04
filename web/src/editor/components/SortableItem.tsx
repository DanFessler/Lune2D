/**
 * @dnd-kit sortable wrapper (ctx-game parity). Use when building draggable inspector rows.
 */
import { useSortable } from "@dnd-kit/sortable";
import { CSS } from "@dnd-kit/utilities";
import type { CSSProperties, ReactNode } from "react";

export function SortableItem({
  id,
  children,
  style,
  data,
  disabled = false,
}: {
  id: string;
  children: ReactNode;
  style?: CSSProperties;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  data?: Record<string, any>;
  disabled?: boolean;
}) {
  const {
    attributes,
    listeners,
    setNodeRef,
    transform,
    isDragging,
    transition,
  } = useSortable({ id, disabled, data: { children, ...data } });

  const itemStyle: CSSProperties = {
    transform: CSS.Translate.toString(transform),
    transition,
    opacity: isDragging ? 0 : 1,
    ...style,
  };

  return (
    <div
      ref={setNodeRef}
      style={itemStyle}
      {...attributes}
      {...listeners}
    >
      {children}
    </div>
  );
}

export default SortableItem;
