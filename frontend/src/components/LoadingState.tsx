export function LoadingState({ label = "Cargando…" }: { label?: string }): JSX.Element {
  return <p className="status" aria-live="polite">{label}</p>;
}
