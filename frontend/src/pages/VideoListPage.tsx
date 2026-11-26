import { keepPreviousData, useInfiniteQuery } from "@tanstack/react-query";
import { useEffect, useMemo, useRef, useState } from "react";
import { useLocation, useSearchParams } from "react-router-dom";
import { metadataClient } from "../api/metadataClient";
import { useAuth } from "../auth/AuthContext";
import { ErrorState } from "../components/ErrorState";
import { LoadingState } from "../components/LoadingState";
import { VideoCard } from "../components/VideoCard";
import { VideoListControls } from "../components/VideoListControls";
import type { components } from "../types/generated/metadata";

type Video = components["schemas"]["Video"];

export function VideoListPage(): JSX.Element {
  const [params, setParams] = useSearchParams();
  const location = useLocation();
  const { user } = useAuth();
  const owner = location.pathname.endsWith("/mine") ? "me" as const : undefined;
  const search = params.get("q") ?? "";
  const sortBy = (params.get("sortBy") as "title" | "author" | "uploadedAt" | null) ?? "uploadedAt";
  const order = (params.get("order") as "asc" | "desc" | null) ?? "desc";
  const [searchInput, setSearchInput] = useState(search);
  const paramsRef = useRef(params);
  useEffect(() => {
    paramsRef.current = params;
  }, [params]);
  useEffect(() => {
    const timeout = window.setTimeout(() => {
      const next = new URLSearchParams(paramsRef.current);
      if (searchInput) next.set("q", searchInput);
      else next.delete("q");
      next.delete("cursor");
      setParams(next, { replace: true });
    }, 300);
    return () => window.clearTimeout(timeout);
  }, [searchInput, setParams]);
  useEffect(() => {
    const timeout = window.setTimeout(() => setSearchInput(search), 0);
    return () => window.clearTimeout(timeout);
  }, [search]);
  const query = useInfiniteQuery({
    queryKey: ["videos", owner, sortBy, order, search],
    queryFn: ({ pageParam }) => metadataClient.listVideos({ owner, sortBy, order, q: search || undefined, pageSize: 12, cursor: pageParam }),
    initialPageParam: undefined as string | undefined,
    getNextPageParam: (lastPage) => lastPage?.nextCursor ?? undefined,
    retry: false,
    placeholderData: keepPreviousData,
  });
  const videos = useMemo(() => {
    const unique = new Map<string, Video>();
    query.data?.pages.flatMap((page) => page.items).forEach((item) => unique.set(item.id, item));
    return [...unique.values()];
  }, [query.data?.pages]);
  return <section><div className="page-heading"><div><h1>{owner ? "Mis videos" : "Videos"}</h1><p>Explora el catálogo de Tecflix.</p></div></div>
    <VideoListControls search={searchInput} sortBy={sortBy} order={order} onSearchChange={setSearchInput} onSortChange={(value) => { const next = new URLSearchParams(params); next.set("sortBy", value); next.delete("cursor"); setParams(next); }} onOrderChange={(value) => { const next = new URLSearchParams(params); next.set("order", value); next.delete("cursor"); setParams(next); }} />
    <div role="region" aria-label="Resultados de videos" aria-busy={query.isFetching ? "true" : "false"}>
      {query.isPending ? <LoadingState /> : query.isError ? <ErrorState error={query.error} retry={() => void query.refetch()} /> : <>
        <p className="sr-only" aria-live="polite">{videos.length} {videos.length === 1 ? "video encontrado" : "videos encontrados"}</p>
        {videos.length === 0 ? <p className="status">No hay videos para mostrar.</p> : <div className="video-grid">{videos.map((video) => <VideoCard key={video.id} video={video} showOwnerDetails={owner === "me" || video.ownerId === user?.id} />)}</div>}
        {query.hasNextPage && <button className="button" type="button" onClick={() => void query.fetchNextPage()} disabled={query.isFetchingNextPage}>{query.isFetchingNextPage ? "Cargando…" : "Cargar más"}</button>}
      </>}
    </div>
  </section>;
}
