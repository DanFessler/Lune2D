import type { EngineEntity } from "./engineBridge";
import styles from "./EntityListPanel.module.css";

export type EntityListPanelProps = {
  entities: EngineEntity[];
  selectedId: string | null;
  onSelect: (id: EngineEntity["id"]) => void;
};

export function EntityListPanel({
  entities,
  selectedId,
  onSelect,
}: EntityListPanelProps) {
  return (
    <div className={styles.root}>
      <ul className={styles.list}>
        {entities.map((e) => (
          <li key={e.id} className={styles.item}>
            <button
              type="button"
              className={
                e.id === selectedId ? `${styles.row} ${styles.rowActive}` : styles.row
              }
              onClick={() => onSelect(e.id)}
            >
              <span className={styles.name}>{e.name}</span>
              {/* <span className={styles.id}>{e.id}</span> */}
            </button>
          </li>
        ))}
      </ul>
    </div>
  );
}
