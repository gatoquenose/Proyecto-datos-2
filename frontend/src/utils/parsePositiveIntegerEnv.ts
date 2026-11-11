export function parsePositiveIntegerEnv(value: string | undefined, defaultValue: number): number {
  const normalized = value?.trim() ?? "";
  if (!/^\d+$/.test(normalized)) return defaultValue;

  const parsed = Number(normalized);
  return Number.isSafeInteger(parsed) && parsed > 0 ? parsed : defaultValue;
}
