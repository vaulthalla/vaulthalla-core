import { NextRequest, NextResponse } from 'next/server'

const AUTH_ORIGIN =
  process.env.VAULTHALLA_AUTH_ORIGIN ??
  process.env.VAULTHALLA_PREVIEW_ORIGIN ??
  'http://127.0.0.1:36970'

const FETCH_TIMEOUT_MS = 2500

export async function GET(req: NextRequest) {
  const controller = new AbortController()
  const timeout = setTimeout(() => controller.abort(), FETCH_TIMEOUT_MS)

  try {
    const upstream = await fetch(`${AUTH_ORIGIN}/auth/session`, {
      method: 'GET',
      headers: {
        cookie: req.headers.get('cookie') ?? '',
        authorization: req.headers.get('authorization') ?? '',
        'x-forwarded-host':
          req.headers.get('x-forwarded-host') ??
          req.headers.get('host') ??
          '',
        'x-forwarded-proto':
          req.headers.get('x-forwarded-proto') ??
          req.nextUrl.protocol.replace(':', ''),
      },
      cache: 'no-store',
      signal: controller.signal,
    })

    const contentType = upstream.headers.get('content-type') ?? 'text/plain'
    const body = await upstream.text()

    if (!upstream.ok) {
      return NextResponse.json(
        {
          ok: false,
          status: upstream.status,
          error: body || 'session unavailable',
        },
        { status: 401 },
      )
    }

    return new NextResponse(body, {
      status: 200,
      headers: { 'content-type': contentType },
    })
  } catch {
    return NextResponse.json(
      { ok: false, error: 'session check timed out or failed' },
      { status: 401 },
    )
  } finally {
    clearTimeout(timeout)
  }
}
