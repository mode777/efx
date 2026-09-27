export interface Sample {
  id: string;
  origin: 'golden' | 'curated';
  title: string;
  category: string;
  description: string;
  source: string;
  /** Optional asset-pack URL (relative to the site root) the runner mounts
   *  as the resource root before booting the sample. */
  assets?: string;
}

export interface Catalog {
  generated: boolean;
  goldenCount: number;
  curatedCount: number;
  samples: Sample[];
}
