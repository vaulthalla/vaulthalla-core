'use client'

import Link from 'next/link'
import React, { useEffect } from 'react'

import type {
  DashboardAttentionItem,
  DashboardCardSummary,
  DashboardIssueSummary,
  DashboardMetricSummary,
  DashboardSectionSummary,
  DashboardSeverity,
} from '@/models/stats/dashboardOverview'
import { useStatsStore } from '@/stores/statsStore'

type Tone = {
  label: string
  border: string
  bg: string
  text: string
  dot: string
  soft: string
  ring: string
}

const tones: Record<DashboardSeverity, Tone> = {
  healthy: {
    label: 'healthy',
    border: 'border-emerald-300/30',
    bg: 'bg-emerald-400/10',
    text: 'text-emerald-100',
    dot: 'bg-emerald-300',
    soft: 'text-emerald-200/80',
    ring: 'shadow-[0_0_0_1px_rgba(110,231,183,0.12),0_18px_45px_-30px_rgba(52,211,153,0.65)]',
  },
  info: {
    label: 'info',
    border: 'border-cyan-300/30',
    bg: 'bg-cyan-400/10',
    text: 'text-cyan-100',
    dot: 'bg-cyan-300',
    soft: 'text-cyan-200/80',
    ring: 'shadow-[0_0_0_1px_rgba(103,232,249,0.12),0_18px_45px_-30px_rgba(34,211,238,0.65)]',
  },
  warning: {
    label: 'warning',
    border: 'border-amber-300/35',
    bg: 'bg-amber-400/10',
    text: 'text-amber-100',
    dot: 'bg-amber-300',
    soft: 'text-amber-200/80',
    ring: 'shadow-[0_0_0_1px_rgba(252,211,77,0.12),0_18px_45px_-30px_rgba(251,191,36,0.65)]',
  },
  error: {
    label: 'error',
    border: 'border-rose-300/35',
    bg: 'bg-rose-400/10',
    text: 'text-rose-100',
    dot: 'bg-rose-300',
    soft: 'text-rose-200/80',
    ring: 'shadow-[0_0_0_1px_rgba(253,164,175,0.12),0_18px_45px_-30px_rgba(244,63,94,0.65)]',
  },
  unknown: {
    label: 'unknown',
    border: 'border-white/10',
    bg: 'bg-white/5',
    text: 'text-white/70',
    dot: 'bg-white/35',
    soft: 'text-white/55',
    ring: 'shadow-[0_18px_45px_-35px_rgba(255,255,255,0.2)]',
  },
  unavailable: {
    label: 'unavailable',
    border: 'border-white/10',
    bg: 'bg-white/5',
    text: 'text-white/55',
    dot: 'bg-white/25',
    soft: 'text-white/45',
    ring: 'shadow-[0_18px_45px_-35px_rgba(255,255,255,0.18)]',
  },
}

function toneFor(severity: DashboardSeverity): Tone {
  return tones[severity] ?? tones.unknown
}

function formatCheckedAt(value: number | string | null, fallback: number | null): string {
  if (typeof value === 'number' && Number.isFinite(value) && value > 0) {
    const ms = value > 1_000_000_000_000 ? value : value * 1000
    return new Date(ms).toLocaleTimeString()
  }

  if (typeof value === 'string' && value.length > 0) {
    const parsed = Date.parse(value)
    if (Number.isFinite(parsed)) return new Date(parsed).toLocaleTimeString()
  }

  return fallback ? new Date(fallback).toLocaleTimeString() : 'pending'
}

function issueGlyph(severity: DashboardSeverity): string {
  if (severity === 'error') return '!'
  if (severity === 'warning') return '!'
  return 'i'
}

const LiveBadge = ({
  loading,
  error,
  lastUpdated,
}: {
  loading: boolean
  error: string | null
  lastUpdated: number | null
}) => (
  <div className="inline-flex items-center gap-2 rounded-full border border-white/10 bg-white/5 px-3 py-1 text-xs text-white/70 backdrop-blur">
    <span
      className={[
        'h-1.5 w-1.5 rounded-full',
        error ? 'bg-rose-400/80'
        : loading ? 'bg-amber-300/80'
        : 'bg-emerald-400/80',
      ].join(' ')}
    />
    {error ?
      'error'
    : loading ?
      'updating...'
    : 'live'}
    {lastUpdated ?
      <span className="text-white/35">·</span>
    : null}
    {lastUpdated ?
      <span className="text-white/50">{new Date(lastUpdated).toLocaleTimeString()}</span>
    : null}
  </div>
)

function SeverityBadge({ severity }: { severity: DashboardSeverity }) {
  const tone = toneFor(severity)
  return (
    <span className={['inline-flex items-center gap-2 rounded-full border px-2.5 py-1 text-xs', tone.border, tone.bg, tone.text].join(' ')}>
      <span className={['h-1.5 w-1.5 rounded-full', tone.dot].join(' ')} />
      {tone.label}
    </span>
  )
}

function IssueRow({ issue, link = true }: { issue: DashboardIssueSummary | DashboardAttentionItem; link?: boolean }) {
  const tone = toneFor(issue.severity)
  const content = (
    <div className={['flex items-start gap-3 rounded-2xl border px-3 py-2', tone.border, tone.bg].join(' ')}>
      <span
        className={[
          'mt-0.5 inline-flex h-5 w-5 shrink-0 items-center justify-center rounded-full text-xs font-bold',
          tone.bg,
          tone.text,
        ].join(' ')}>
        {issueGlyph(issue.severity)}
      </span>
      <div className="min-w-0">
        {'title' in issue ?
          <div className="mb-0.5 text-xs font-semibold text-white/70">{issue.title}</div>
        : null}
        <div className="text-sm text-white/80">{issue.message}</div>
      </div>
    </div>
  )

  return link && issue.href ?
      <Link href={issue.href} className="block transition hover:brightness-125">
        {content}
      </Link>
    : content
}

function MetricPill({ metric, link = true }: { metric: DashboardMetricSummary; link?: boolean }) {
  const tone = toneFor(metric.tone)
  const body = (
    <div className={['rounded-2xl border px-3 py-2', tone.border, tone.bg].join(' ')}>
      <div className="text-[11px] text-white/45 uppercase">{metric.label}</div>
      <div className={['mt-1 text-sm font-semibold', tone.text].join(' ')}>{metric.value || 'unknown'}</div>
    </div>
  )

  return link && metric.href ?
      <Link href={metric.href} className="transition hover:brightness-125">
        {body}
      </Link>
    : body
}

function OverviewShell({
  children,
  className,
}: {
  children: React.ReactNode
  className?: string
}) {
  return (
    <section
      className={[
        'relative overflow-hidden rounded-3xl border border-white/10 bg-zinc-950/55 p-5 shadow-[0_20px_60px_-25px_rgba(0,0,0,0.9)] backdrop-blur-xl',
        'before:pointer-events-none before:absolute before:inset-0 before:bg-[radial-gradient(900px_circle_at_15%_10%,rgba(255,255,255,0.08),transparent_45%),radial-gradient(700px_circle_at_85%_25%,rgba(56,189,248,0.09),transparent_55%)]',
        className ?? '',
      ].join(' ')}>
      <div className="relative z-10">{children}</div>
    </section>
  )
}

function SectionCard({ section }: { section: DashboardSectionSummary }) {
  const tone = toneFor(section.severity)
  return (
    <Link href={section.href} className="block h-full transition hover:-translate-y-0.5 hover:brightness-110">
      <div className={['h-full rounded-3xl border bg-zinc-950/45 p-4 backdrop-blur', tone.border, tone.ring].join(' ')}>
        <div className="flex items-start justify-between gap-3">
          <div className="min-w-0">
            <div className="text-sm font-semibold text-white/90">{section.title}</div>
            <div className="mt-1 text-xs text-white/50">{section.description}</div>
          </div>
          <SeverityBadge severity={section.severity} />
        </div>
        <p className="mt-4 min-h-10 text-sm text-white/70">{section.summary}</p>
        <div className="mt-4 grid grid-cols-2 gap-2">
          {section.metrics.slice(0, 4).map(metric => (
            <MetricPill key={`${section.id}-${metric.key}-${metric.label}`} metric={metric} link={false} />
          ))}
        </div>
      </div>
    </Link>
  )
}

function SummaryCard({ card }: { card: DashboardCardSummary }) {
  const tone = toneFor(card.severity)
  return (
    <Link href={card.href} className="block h-full transition hover:-translate-y-0.5 hover:brightness-110">
      <article className={['h-full rounded-3xl border bg-zinc-950/45 p-4 backdrop-blur', tone.border, tone.ring].join(' ')}>
        <div className="flex items-start justify-between gap-3">
          <div className="min-w-0">
            <div className="text-sm font-semibold text-white/90">{card.title}</div>
            <div className="mt-1 line-clamp-2 text-xs text-white/50">{card.description}</div>
          </div>
          <SeverityBadge severity={card.severity} />
        </div>
        <p className="mt-4 min-h-10 text-sm text-white/70">{card.available ? card.summary : card.unavailable_reason}</p>
        <div className="mt-4 grid grid-cols-1 gap-2 sm:grid-cols-3">
          {card.metrics.slice(0, 3).map(metric => (
            <MetricPill key={`${card.id}-${metric.key}`} metric={metric} link={false} />
          ))}
        </div>
        {card.errors.length || card.warnings.length ?
          <div className="mt-4 space-y-2">
            {[...card.errors, ...card.warnings].slice(0, 2).map(issue => (
              <IssueRow key={`${card.id}-${issue.code}`} issue={issue} link={false} />
            ))}
          </div>
        : null}
      </article>
    </Link>
  )
}

export default function DashboardOverviewComponent({ intervalMs = 7500 }: { intervalMs?: number }) {
  const wrapper = useStatsStore(s => s.dashboardOverview)
  const startPolling = useStatsStore(s => s.startDashboardOverviewPolling)
  const stopPolling = useStatsStore(s => s.stopDashboardOverviewPolling)

  useEffect(() => {
    startPolling(intervalMs)
    return () => stopPolling()
  }, [startPolling, stopPolling, intervalMs])

  const overview = wrapper.data
  const tone = toneFor(overview.overall_status)
  const checkedAt = formatCheckedAt(overview.checked_at, wrapper.lastUpdated)

  return (
    <div className="space-y-6">
      <OverviewShell className={tone.ring}>
        <div className="flex flex-col gap-5 lg:flex-row lg:items-center lg:justify-between">
          <div className="min-w-0">
            <div className="text-xs font-semibold tracking-[0.18em] text-cyan-200/65 uppercase">Admin Dashboard</div>
            <div className="mt-3 flex flex-wrap items-center gap-3">
              <h1 className="text-2xl font-semibold text-white">Health Command Center</h1>
              <SeverityBadge severity={overview.overall_status} />
            </div>
            <p className="mt-2 max-w-3xl text-sm text-white/60">
              Runtime, filesystem, storage, operations, and trend posture at a glance.
            </p>
          </div>

          <div className="flex shrink-0 flex-wrap items-center gap-2">
            <div className={['rounded-2xl border px-3 py-2', tone.border, tone.bg].join(' ')}>
              <div className="text-[11px] text-white/45 uppercase">Warnings</div>
              <div className={['text-lg font-semibold', tone.text].join(' ')}>{overview.warning_count}</div>
            </div>
            <div className={['rounded-2xl border px-3 py-2', tone.border, tone.bg].join(' ')}>
              <div className="text-[11px] text-white/45 uppercase">Errors</div>
              <div className={['text-lg font-semibold', tone.text].join(' ')}>{overview.error_count}</div>
            </div>
            <div className="rounded-2xl border border-white/10 bg-white/5 px-3 py-2 text-xs text-white/55">
              checked {checkedAt}
            </div>
            <LiveBadge loading={wrapper.loading} error={wrapper.error} lastUpdated={wrapper.lastUpdated} />
          </div>
        </div>

        {wrapper.error ?
          <div className="mt-4 rounded-2xl border border-rose-300/30 bg-rose-400/10 px-3 py-2 text-sm text-rose-100">
            {wrapper.error}
          </div>
        : null}
      </OverviewShell>

      <OverviewShell>
        <div className="flex items-center justify-between gap-3">
          <div>
            <div className="text-sm font-semibold text-white/90">Attention Queue</div>
            <div className="mt-1 text-xs text-white/50">Current warnings and errors across dashboard sections.</div>
          </div>
          <span className="rounded-full border border-white/10 bg-white/5 px-2.5 py-1 text-xs text-white/55">
            {overview.attention.length} items
          </span>
        </div>
        <div className="mt-4 grid grid-cols-1 gap-2 lg:grid-cols-2">
          {overview.attention.length ?
            overview.attention.slice(0, 6).map(item => <IssueRow key={`${item.card_id}-${item.code}`} issue={item} />)
          : <div className="rounded-2xl border border-emerald-300/25 bg-emerald-400/10 px-3 py-3 text-sm text-emerald-100">
              No active warnings or errors.
            </div>
          }
        </div>
      </OverviewShell>

      <div className="grid grid-cols-1 gap-4 lg:grid-cols-5">
        {overview.sections.map(section => (
          <SectionCard key={section.id} section={section} />
        ))}
      </div>

      <div className="grid grid-cols-1 gap-4 xl:grid-cols-2">
        {overview.cards.map(card => (
          <SummaryCard key={card.id} card={card} />
        ))}
      </div>
    </div>
  )
}
