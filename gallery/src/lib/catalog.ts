export interface Sample {
  id: string;
  origin: 'golden' | 'curated';
  title: string;
  category: string;
  description: string;
  source: string;
}

export interface Catalog {
  generated: boolean;
  goldenCount: number;
  curatedCount: number;
  samples: Sample[];
}
