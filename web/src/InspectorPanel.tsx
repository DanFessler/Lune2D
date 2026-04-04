import type { EngineEntity } from "./engineBridge";
import styles from "./InspectorPanel.module.css";

export type InspectorPanelProps = {
  entity: EngineEntity | null;
};

function FieldFloat({
  label,
  value,
}: {
  label: string;
  value: number;
}) {
  return (
    <div className={styles.row}>
      <span className={styles.label}>{label}</span>
      <input
        className={`${styles.input} ${styles.inputSpan2}`}
        type="number"
        step="any"
        readOnly
        value={Number.isFinite(value) ? value : 0}
      />
    </div>
  );
}

function FieldVec2({
  label,
  aLabel,
  bLabel,
  a,
  b,
}: {
  label: string;
  aLabel: string;
  bLabel: string;
  a: number;
  b: number;
}) {
  return (
    <div className={styles.row}>
      <span className={styles.label}>{label}</span>
      <input
        className={styles.input}
        type="number"
        step="any"
        readOnly
        value={Number.isFinite(a) ? a : 0}
        aria-label={`${label} ${aLabel}`}
      />
      <input
        className={styles.input}
        type="number"
        step="any"
        readOnly
        value={Number.isFinite(b) ? b : 0}
        aria-label={`${label} ${bLabel}`}
      />
    </div>
  );
}

export function InspectorPanel({ entity }: InspectorPanelProps) {
  if (!entity) {
    return (
      <div className={styles.root}>
        <header className={styles.header}>
          <h1 className={styles.title}>Inspector</h1>
          <p className={styles.hint}>Select an entity to edit its transform.</p>
        </header>
      </div>
    );
  }

  return (
    <div className={styles.root}>
      <header className={styles.header}>
        <h1 className={styles.title}>Transform</h1>
        <p className={styles.entityName}>{entity.name}</p>
        <p className={styles.entityId}>{entity.id}</p>
      </header>

      <div className={styles.grid}>
        <FieldVec2
          label="Position"
          aLabel="X"
          bLabel="Y"
          a={entity.x}
          b={entity.y}
        />
        <FieldFloat label="Angle" value={entity.angle} />
        <FieldVec2
          label="Velocity"
          aLabel="Vx"
          bLabel="Vy"
          a={entity.vx}
          b={entity.vy}
        />
      </div>
    </div>
  );
}
