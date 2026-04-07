import { useEffect, useState } from "react";
import type { EngineEntity } from "./engineBridge";
import {
  engineEntityListsSnapshotEqual,
  installEngineEntityBridge,
} from "./engineBridge";

export function useEngineEntities() {
  const [entities, setEntities] = useState<EngineEntity[]>([]);

  useEffect(() => {
    return installEngineEntityBridge((list) => {
      setEntities((prev) =>
        engineEntityListsSnapshotEqual(prev, list) ? prev : list,
      );
    });
  }, []);

  return entities;
}
