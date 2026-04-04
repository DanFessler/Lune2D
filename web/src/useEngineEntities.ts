import { useEffect, useState } from "react";
import type { EngineEntity } from "./engineBridge";
import { installEngineEntityBridge } from "./engineBridge";

export function useEngineEntities() {
  const [entities, setEntities] = useState<EngineEntity[]>([]);

  useEffect(() => {
    return installEngineEntityBridge(setEntities);
  }, []);

  return entities;
}
