interface QuotaBarProps {
  quotaBytes: number;
  usedBytes: number;
}

const formatBytes = (bytes: number): string => `${(bytes / 1073741824).toFixed(2)} GB`;

export function QuotaBar({ quotaBytes, usedBytes }: QuotaBarProps): JSX.Element {
  const percentage = quotaBytes === 0 ? 0 : Math.min(100, (usedBytes / quotaBytes) * 100);
  return <section aria-label="Cuota de almacenamiento" className="quota">
    <div className="quota-label"><span>Cuota</span><span>{formatBytes(usedBytes)} / {formatBytes(quotaBytes)}</span></div>
    <div className="quota-track"><div className="quota-fill" style={{ width: `${percentage}%` }} /></div>
    <small>{formatBytes(Math.max(0, quotaBytes - usedBytes))} disponibles</small>
  </section>;
}
