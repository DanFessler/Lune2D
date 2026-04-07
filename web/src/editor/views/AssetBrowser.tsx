import {
  useEffect,
  useState,
  type CSSProperties,
  type MouseEvent as ReactMouseEvent,
} from "react";
import { FaAngleRight, FaFile, FaFileCode, FaFileImage, FaFolder } from "react-icons/fa";
import { BsFilePlayFill } from "react-icons/bs";
import { listProjectDir, type ProjectDirEntry } from "../../projectFileBridge";
import { isSceneAssetPath } from "../../sceneAssetPath";
import styles from "./AssetBrowser.module.css";

/** Mirrors ctx-game AssetBrowser `File` shape; filled from `listProjectDir`. */
export type AssetBrowserFile = {
  name: string;
  path: string;
  isDirectory: boolean;
  updatedAt: string;
  size: number;
  extension: string | undefined;
  subExtension: string | undefined;
  thumbnail: string | undefined;
};

function projectEntryToAssetFile(e: ProjectDirEntry): AssetBrowserFile {
  const { name } = e;
  let extension: string | undefined;
  let subExtension: string | undefined;
  if (e.isDirectory) {
    extension = undefined;
    subExtension = undefined;
  } else if (name.toLowerCase().endsWith(".scene.json")) {
    extension = "json";
    subExtension = "scene";
  } else {
    const i = name.lastIndexOf(".");
    extension = i >= 0 ? name.slice(i + 1).toLowerCase() : undefined;
    subExtension = undefined;
  }
  return {
    name: e.name,
    path: e.path,
    isDirectory: e.isDirectory,
    updatedAt: e.mtime ?? "",
    size: e.size,
    extension,
    subExtension,
    thumbnail: e.thumbnail,
  };
}

export type AssetBrowserProps = {
  /** Bump to re-fetch the current folder from disk (e.g. after an external save). */
  refreshKey?: number;
  /** First breadcrumb segment; click always navigates to project root (`listProjectDir("")`). */
  rootCrumbLabel?: string;
  /** Project-relative file opened in the text editor (`readProjectFile`). */
  onOpenEntry: (relativePath: string) => void;
  /** `*.scene.json` / `*.scene` → native scene load (bridge). */
  onLoadScene: (relativePath: string) => void;
};

function AssetBrowser({
  refreshKey = 0,
  rootCrumbLabel = "project",
  onOpenEntry,
  onLoadScene,
}: AssetBrowserProps) {
  const [path, setPath] = useState<string[]>([]);
  const [assets, setAssets] = useState<AssetBrowserFile[]>([]);
  const [loadError, setLoadError] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    const dirArg = path.length ? path.join("/") : "";
    setLoadError(null);
    void listProjectDir(dirArg)
      .then((entries) => {
        if (!cancelled) setAssets(entries.map(projectEntryToAssetFile));
      })
      .catch((e) => {
        if (!cancelled) {
          setAssets([]);
          setLoadError(e instanceof Error ? e.message : String(e));
        }
      });
    return () => {
      cancelled = true;
    };
  }, [path, refreshKey]);

  return (
    <FileBrowser
      files={assets}
      path={path}
      setPath={setPath}
      rootCrumbLabel={rootCrumbLabel}
      onOpenEntry={onOpenEntry}
      onLoadScene={onLoadScene}
      loadError={loadError}
    />
  );
}

type FileBrowserProps = {
  files: AssetBrowserFile[];
  path: string[];
  setPath: (path: string[]) => void;
  rootCrumbLabel: string;
  onOpenEntry: (relativePath: string) => void;
  onLoadScene: (relativePath: string) => void;
  loadError: string | null;
};

function FileBrowser({
  files,
  path,
  setPath,
  rootCrumbLabel,
  onOpenEntry,
  onLoadScene,
  loadError,
}: FileBrowserProps) {
  const [selectedFiles, setSelectedFiles] = useState<AssetBrowserFile[]>([]);
  const [madWidth, setMadWidth] = useState(64);

  function handleFileSelect(file: AssetBrowserFile, e: ReactMouseEvent<HTMLDivElement>) {
    if (e.shiftKey || e.ctrlKey) {
      const index = selectedFiles.findIndex((f) => f.path === file.path);
      const newSelectedFiles = [...selectedFiles];
      if (index === -1) {
        newSelectedFiles.push(file);
      } else {
        newSelectedFiles.splice(index, 1);
      }
      setSelectedFiles(newSelectedFiles);
    } else {
      setSelectedFiles([file]);
    }
  }

  return (
    <div className={`${styles.container} asset-browser-root`}>
      {loadError ? <div className={styles.error}>{loadError}</div> : null}
      <div className={styles.pathBar}>
        <div
          className={styles.pathBarItem}
          onClick={() => setPath([])}
        >
          {rootCrumbLabel}
        </div>
        {path.map((part, index) => (
          <div
            key={`${part}-${index}`}
            onClick={() => setPath(path.slice(0, index + 1))}
            className={styles.pathBarItem}
          >
            <span className={styles.pathSeparator}>
              <FaAngleRight />
            </span>
            {part}
          </div>
        ))}
        <div className={styles.pathSpacer} />
        <div className={styles.pathBarInputContainer}>
          <input
            className={styles.pathBarInput}
            type="range"
            min={32}
            max={96}
            value={madWidth}
            onChange={(e) => setMadWidth(parseInt(e.target.value, 10))}
          />
          <span>aA</span>
        </div>
      </div>
      <div
        className={styles.gridContainer}
        style={{
          gridTemplateColumns: `repeat(auto-fill, ${madWidth}px)`,
        }}
        onMouseDown={(e) => {
          if (e.target === e.currentTarget) {
            setSelectedFiles([]);
          }
        }}
      >
        {files.map((asset) => (
          <FileRow
            key={asset.path}
            asset={asset}
            selected={selectedFiles.some((f) => f.path === asset.path)}
            handleFileSelect={handleFileSelect}
            path={path}
            setPath={setPath}
            madWidth={madWidth}
            onOpenEntry={onOpenEntry}
            onLoadScene={onLoadScene}
          />
        ))}
      </div>
    </div>
  );
}

function FileRow({
  asset,
  selected,
  handleFileSelect,
  setPath,
  madWidth = 64,
  path,
  onOpenEntry,
  onLoadScene,
}: {
  asset: AssetBrowserFile;
  selected: boolean;
  handleFileSelect: (file: AssetBrowserFile, e: ReactMouseEvent<HTMLDivElement>) => void;
  path: string[];
  setPath: (path: string[]) => void;
  madWidth: number;
  onOpenEntry: (relativePath: string) => void;
  onLoadScene: (relativePath: string) => void;
}) {
  return (
    <div
      onMouseDown={(e) => handleFileSelect(asset, e)}
      onDoubleClick={() => {
        if (asset.isDirectory) {
          setPath([...path, asset.name]);
          return;
        }
        if (isSceneAssetPath(asset.path)) {
          onLoadScene(asset.path);
          return;
        }
        onOpenEntry(asset.path);
      }}
      className={styles.fileContainer}
      style={{
        width: madWidth,
        backgroundColor: selected
          ? "var(--dockable-colors-selected)"
          : "transparent",
      }}
    >
      <div className={styles.fileIcon}>
        <FileIcon file={asset} />
      </div>
      <div className={styles.fileName}>{asset.name.split(".")[0]}</div>
    </div>
  );
}

function FileIcon({ file }: { file: AssetBrowserFile }) {
  const { isDirectory, extension, subExtension, thumbnail } = file;

  function getFileIcon():
    | typeof FaFolder
    | typeof FaFile
    | typeof FaFileCode
    | typeof FaFileImage
    | typeof BsFilePlayFill
    | ReturnType<typeof FileThumbnailHOC> {
    if (isDirectory) {
      return FaFolder;
    }

    switch (extension) {
      case "png":
        return thumbnail ? FileThumbnailHOC(thumbnail) : FaFileImage;
      case "jpg":
      case "jpeg":
      case "gif":
      case "bmp":
      case "tiff":
      case "ico":
      case "webp":
        return FaFileImage;
      case "ts":
        return FaFileCode;
      case "scene":
        return BsFilePlayFill;
      case "json":
        switch (subExtension) {
          case "scene":
            return BsFilePlayFill;
          default:
            return FaFile;
        }
      default:
        return FaFile;
    }
  }

  const Icon = getFileIcon();
  return <Icon style={{ width: "100%", height: "100%" }} />;
}

function FileThumbnailHOC(thumbnail: string | undefined) {
  return function FileThumbnail({ style }: { style: CSSProperties }) {
    return (
      <img
        src={`data:image/png;base64,${thumbnail}`}
        alt=""
        style={{
          borderRadius: "4px",
          boxShadow: "0 1px 3px rgba(0, 0, 0, .25)",
          ...style,
        }}
      />
    );
  };
}

export default AssetBrowser;
