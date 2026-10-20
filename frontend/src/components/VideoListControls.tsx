interface VideoListControlsProps {
  search: string;
  sortBy: "title" | "author" | "uploadedAt";
  order: "asc" | "desc";
  onSearchChange: (value: string) => void;
  onSortChange: (value: "title" | "author" | "uploadedAt") => void;
  onOrderChange: (value: "asc" | "desc") => void;
}

export function VideoListControls({ search, sortBy, order, onSearchChange, onSortChange, onOrderChange }: VideoListControlsProps): JSX.Element {
  return <div className="video-list-controls">
    <label htmlFor="video-search">Buscar videos</label>
    <input id="video-search" type="search" value={search} placeholder="Buscar…" onChange={(event) => onSearchChange(event.target.value)} />
    <label htmlFor="video-sort">Ordenar por</label>
    <select id="video-sort" value={sortBy} onChange={(event) => onSortChange(event.target.value as VideoListControlsProps["sortBy"])}>
      <option value="uploadedAt">Fecha de carga</option>
      <option value="title">Título</option>
      <option value="author">Autor</option>
    </select>
    <label htmlFor="video-order">Dirección</label>
    <select id="video-order" value={order} onChange={(event) => onOrderChange(event.target.value as VideoListControlsProps["order"])}>
      <option value="desc">Descendente</option>
      <option value="asc">Ascendente</option>
    </select>
  </div>;
}
