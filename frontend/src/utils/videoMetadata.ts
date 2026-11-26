export const VIDEO_METADATA_LIMITS = {
  title: { minLength: 1, maxLength: 200 },
  description: { maxLength: 5000 },
  author: { maxLength: 200 },
  topic: { maxLength: 100 },
  tags: { maxItems: 30, maxLength: 50 },
} as const;

export type VideoMetadataField = "title" | "description" | "author" | "topic" | "tags";
export type VideoMetadataErrors = Partial<Record<VideoMetadataField, string>>;

export interface EditableVideoMetadata {
  title: string;
  description: string;
  author: string;
  topic: string;
  tags: string[];
}

export function validateVideoMetadata(metadata: EditableVideoMetadata): VideoMetadataErrors {
  const errors: VideoMetadataErrors = {};
  if (!metadata.title.trim() || metadata.title.length > VIDEO_METADATA_LIMITS.title.maxLength) {
    errors.title = "El título es obligatorio y debe tener entre 1 y 200 caracteres.";
  }
  if (metadata.description.length > VIDEO_METADATA_LIMITS.description.maxLength) {
    errors.description = "La descripción no puede superar 5000 caracteres.";
  }
  if (metadata.author.length > VIDEO_METADATA_LIMITS.author.maxLength) {
    errors.author = "El autor no puede superar 200 caracteres.";
  }
  if (metadata.topic.length > VIDEO_METADATA_LIMITS.topic.maxLength) {
    errors.topic = "El tema no puede superar 100 caracteres.";
  }
  if (metadata.tags.length > VIDEO_METADATA_LIMITS.tags.maxItems
    || metadata.tags.some((tag) => tag.length > VIDEO_METADATA_LIMITS.tags.maxLength)) {
    errors.tags = "Las etiquetas deben ser hasta 30 y tener máximo 50 caracteres.";
  }
  return errors;
}
