#pragma once

class ScalarEkf {
public:
    ScalarEkf(double q = 1e-6, double r = 1e-4, double p0 = 1.0)
        : m_q(q > 1e-15 ? q : 1e-15)
        , m_r(r > 1e-15 ? r : 1e-15)
        , m_p0(p0 > 1e-15 ? p0 : 1e-15)
        , m_initialized(false)
        , m_x(0.0)
        , m_p(m_p0)
    {
    }

    void configure(double q, double r, double p0)
    {
        m_q = q > 1e-15 ? q : 1e-15;
        m_r = r > 1e-15 ? r : 1e-15;
        m_p0 = p0 > 1e-15 ? p0 : 1e-15;
    }

    void reset()
    {
        m_initialized = false;
        m_x = 0.0;
        m_p = m_p0;
    }

    double update(double z)
    {
        if (!m_initialized) {
            m_x = z;
            m_p = m_p0;
            m_initialized = true;
            return m_x;
        }

        const double xPred = m_x;
        const double pPred = m_p + m_q;
        double s = pPred + m_r;
        if (s <= 1e-15) {
            s = 1e-15;
        }

        const double k = pPred / s;
        m_x = xPred + k * (z - xPred);
        m_p = (1.0 - k) * pPred;
        if (m_p < 1e-15) {
            m_p = 1e-15;
        }

        return m_x;
    }

private:
    double m_q;
    double m_r;
    double m_p0;
    bool m_initialized;
    double m_x;
    double m_p;
};
