import { useState } from "react";

interface TagInputProps {
  value: string[];
  onChange: (tags: string[]) => void;
  maxTags?: number;
  maxLength?: number;
  errorId?: string;
  invalid?: boolean;
}

export function TagInput({ value, onChange, maxTags = 30, maxLength = 50, errorId, invalid }: TagInputProps): JSX.Element {
  const [draft, setDraft] = useState("");

  const add = (raw: string): void => {
    const tag = raw.trim();
    if (!tag || tag.length > maxLength || value.length >= maxTags || value.includes(tag)) {
      setDraft("");
      return;
    }
    onChange([...value, tag]);
    setDraft("");
  };

  return <div className="tag-input">
    <label htmlFor="video-tags">Etiquetas</label>
    <input
      id="video-tags"
      value={draft}
      maxLength={maxLength}
      onChange={(event) => setDraft(event.target.value)}
      onKeyDown={(event) => {
        if (event.key === "Enter" || event.key === ",") {
          event.preventDefault();
          add(draft);
        }
      }}
      placeholder="Escribe una etiqueta y pulsa Enter"
      aria-describedby={["video-tags-help", errorId].filter(Boolean).join(" ")}
      aria-invalid={invalid || undefined}
    />
    <small id="video-tags-help">Hasta {maxTags} etiquetas de {maxLength} caracteres.</small>
    {value.length > 0 && <ul className="tag-list" aria-label="Etiquetas añadidas">
      {value.map((tag) => <li key={tag} className="tag-chip">
        <span>{tag}</span>
        <button type="button" aria-label={`Quitar etiqueta ${tag}`} onClick={() => onChange(value.filter((item) => item !== tag))}>×</button>
      </li>)}
    </ul>}
  </div>;
}
