import { useInfiniteQuery } from "@tanstack/react-query";
import { useEffect, useMemo, useState } from "react";
import { useLocation, useSearchParams } from "react-router-dom";
import { metadataClient } from "../api/metadataClient";
import { useAuth } from "../auth/AuthContext";
import { ErrorState } from "../components/ErrorState";
import { LoadingState } from "../components/LoadingState";
import { VideoCard } from "../components/VideoCard";
import { VideoListControls } from "../components/VideoListControls";

export function VideoListPage(): JSX.Element {
  const [params, setParams] = useSearchParams();
  const location = useLocation();
  const { user } = useAuth();
  const owner = location.pathname.endsWith("/mine") ? "me" as const : undefined;
  const search = params.get("q") ?? "";
  const sortBy = (params.get("sortBy") as "title" | "author" | "uploadedAt" | null) ?? "uploadedAt";
  const order = (params.get("order") as "asc" | "desc" | null) ?? "desc";
  const [searchInput, setSearchInput] = useState(search);
  const [debouncedSearch, setDebouncedSearch] = useState(search);
  useEffect(() => {
    const timeout = window.setTimeout(() => {
      setDebouncedSearch(searchInput);
      const next = new URLSearchParams(params);
      if (searchInput) next.set("q", searchInput);
      else next.delete("q");
      next.delete("cursor");
      setParams(next, { replace: true });
    }, 300);
    return () => window.clearTimeout(timeout);
  }, [searchInput]);
  useEffect(() => setSearchInput(search), [search]);
  const query = useInfiniteQuery({
    queryKey: ["videos", owner, sortBy, order, debouncedSearch],
    queryFn: ({ pageParam }) => metadataClient.listVideos({ owner, sortBy, order, q: debouncedSearch || undefined, pageSize: 12, cursor: pageParam }),
    initialPageParam: undefined as string | undefined,
    getNextPageParam: (lastPage) => lastPage.nextCursor ?? undefined,
  });
  const videos = useMemo(() => query.data?.pages.flatMap((page) => page.items) ?? [], [query.data?.pages]);
  if (query.isPending) return <LoadingState />;
  if (query.isError) return <ErrorState error={query.error} retry={() => void query.refetch()} />;
  return <section><div className="page-heading"><div><h1>{owner ? "Mis videos" : "Videos"}</h1><p>Explora el catálogo de Tecflix.</p></div></div>
    <VideoListControls search={searchInput} sortBy={sortBy} order={order} onSearchChange={setSearchInput} onSortChange={(value) => { const next = new URLSearchParams(params); next.set("sortBy", value); next.delete("cursor"); setParams(next); }} onOrderChange={(value) => { const next = new URLSearchParams(params); next.set("order", value); next.delete("cursor"); setParams(next); }} />
    {videos.length === 0 ? <p className="status">No hay videos para mostrar.</p> : <div className="video-grid">{videos.map((video) => <VideoCard key={video.id} video={video} showOwnerDetails={owner === "me" || video.ownerId === user?.id} />)}</div>}
    {query.hasNextPage && <button className="button" type="button" onClick={() => void query.fetchNextPage()} disabled={query.isFetchingNextPage}>{query.isFetchingNextPage ? "Cargando…" : "Cargar más"}</button>}
  </section>;
}
