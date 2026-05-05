'use client'

import Link from 'next/link'
import React, { useEffect } from 'react'

import type {
  DashboardCardSummary,
  DashboardMetricSummary,
  DashboardSectionSummary,
} from '@/models/stats/dashboardOverview'
import { DashboardIssueList, DashboardIssueSummaryLine } from '@/components/dashboard/DashboardIssueList'
import { DashboardSeverityBadge, DashboardSeverityIcon } from '@/components/dashboard/DashboardSeverityBadge'
import {
  dashboardSeverityTone,
  sortDashboardIssues,
} from '@/components/dashboard/dashboardSeverity'
import { useStatsStore } from '@/stores/statsStore'

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

function MetricPill({ metric, link = true }: { metric: DashboardMetricSummary; link?: boolean }) {
  const tone = dashboardSeverityTone(metric.tone)
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
  const tone = dashboardSeverityTone(section.severity)
  const firstIssue = sortDashboardIssues([...section.errors, ...section.warnings])[0]

  return (
    <Link href={section.href} className="block h-full transition hover:-translate-y-0.5 hover:brightness-110">
      <div
        className={[
          'h-full rounded-3xl border bg-zinc-950/45 p-4 backdrop-blur',
          tone.border,
          tone.ring,
          section.error_count > 0 || section.warning_count > 0 ? 'ring-1 ring-inset' : '',
          section.error_count > 0 ? 'ring-rose-300/15'
          : section.warning_count > 0 ? 'ring-amber-300/15'
          : 'ring-white/0',
        ].join(' ')}>
        <div className="flex items-start justify-between gap-3">
          <div className="min-w-0">
            <div className="flex items-center gap-2 text-sm font-semibold text-white/90">
              <DashboardSeverityIcon severity={section.severity} className={['h-4 w-4', tone.text].join(' ')} />
              {section.title}
            </div>
            <div className="mt-1 text-xs text-white/50">{section.description}</div>
            <DashboardIssueSummaryLine
              severity={section.error_count > 0 ? 'error' : section.warning_count > 0 ? 'warning' : section.severity}
              errorCount={section.error_count}
              warningCount={section.warning_count}
              firstIssue={firstIssue}
            />
          </div>
          <DashboardSeverityBadge
            severity={section.severity}
            errorCount={section.error_count}
            warningCount={section.warning_count}
            showCount
          />
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
  const tone = dashboardSeverityTone(card.severity)
  const errors = card.errors.length
  const warnings = card.warnings.length
  const firstIssue = sortDashboardIssues([...card.errors, ...card.warnings])[0]

  return (
    <Link href={card.href} className="block h-full transition hover:-translate-y-0.5 hover:brightness-110">
      <article
        className={[
          'h-full rounded-3xl border bg-zinc-950/45 p-4 backdrop-blur',
          tone.border,
          tone.ring,
          errors > 0 || warnings > 0 ? 'ring-1 ring-inset' : '',
          errors > 0 ? 'ring-rose-300/15'
          : warnings > 0 ? 'ring-amber-300/15'
          : 'ring-white/0',
        ].join(' ')}>
        <div className="flex items-start justify-between gap-3">
          <div className="min-w-0">
            <div className="flex items-center gap-2 text-sm font-semibold text-white/90">
              <DashboardSeverityIcon severity={card.severity} className={['h-4 w-4', tone.text].join(' ')} />
              {card.title}
            </div>
            <div className="mt-1 line-clamp-2 text-xs text-white/50">{card.description}</div>
            <DashboardIssueSummaryLine
              severity={errors > 0 ? 'error' : warnings > 0 ? 'warning' : card.severity}
              errorCount={errors}
              warningCount={warnings}
              firstIssue={firstIssue}
            />
          </div>
          <DashboardSeverityBadge severity={card.severity} errorCount={errors} warningCount={warnings} showCount />
        </div>
        <p className="mt-4 min-h-10 text-sm text-white/70">{card.available ? card.summary : card.unavailable_reason}</p>
        <div className="mt-4 grid grid-cols-1 gap-2 sm:grid-cols-3">
          {card.metrics.slice(0, 3).map(metric => (
            <MetricPill key={`${card.id}-${metric.key}`} metric={metric} link={false} />
          ))}
        </div>
        {card.errors.length || card.warnings.length ?
          <div className="mt-4">
            <DashboardIssueList issues={[...card.errors, ...card.warnings]} max={2} link={false} compact />
          </div>
        : null}
        <div className="mt-4 text-xs font-medium text-cyan-100/80">View details {'>'}</div>
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
  const tone = dashboardSeverityTone(overview.overall_status)
  const checkedAt = formatCheckedAt(overview.checked_at, wrapper.lastUpdated)

  return (
    <div className="space-y-6">
      <OverviewShell className={tone.ring}>
        <div className="flex flex-col gap-5 lg:flex-row lg:items-center lg:justify-between">
          <div className="min-w-0">
            <div className="text-xs font-semibold tracking-[0.18em] text-cyan-200/65 uppercase">Admin Dashboard</div>
            <div className="mt-3 flex flex-wrap items-center gap-3">
              <h1 className="text-2xl font-semibold text-white">Health Command Center</h1>
              <DashboardSeverityBadge
                severity={overview.overall_status}
                errorCount={overview.error_count}
                warningCount={overview.warning_count}
                showCount
              />
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
        <div className="mt-4">
          {overview.attention.length ?
            <DashboardIssueList issues={overview.attention} max={6} className="grid grid-cols-1 gap-2 lg:grid-cols-2" />
          : <div className="rounded-2xl border border-emerald-300/25 bg-emerald-400/10 px-3 py-3 text-sm text-emerald-100">
              All monitored systems nominal.
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
